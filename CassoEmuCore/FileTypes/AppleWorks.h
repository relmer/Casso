#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorks
//
//  What the three AppleWorks file types have in common: a word in the
//  6502's order, the text of a cell, a category or a line, ASCII with
//  MouseText in the high bit's half on 3.0, and a paragraph's styled runs.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWorks
{
public:
    static Word          ReadWord   (std::span<const Byte> bytes, size_t at);
    static uint32_t      ReadLong   (std::span<const Byte> bytes, size_t at);
    static std::wstring  DecodeText (std::span<const Byte> bytes);
    static void          AddText    (Paragraph & paragraph, const std::wstring & text, bool bold, bool italic, bool underline);
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksWp
//
//  An AppleWorks word processor file, $1A, as File Type Note $1A sets it
//  out: a 300-byte header, then a record for each screen line, a text line,
//  a carriage return or a command, to two $FF bytes. A text line without a
//  carriage return runs on into the next one's paragraph.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWorksWp
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte    kFileType      = 0x1A;
    static constexpr size_t  kHeaderBytes   = 300;
    static constexpr size_t  kMinVersionAt  = 183;
    static constexpr Byte    kVersion30     = 30;
    static constexpr Byte    kCarriageLine  = 0xD0;
    static constexpr Byte    kRuler         = 0xFF;
    static constexpr Byte    kEndOfFile     = 0xFF;

    static constexpr Byte    kRightAligned  = 0xD7;
    static constexpr Byte    kJustified     = 0xDF;
    static constexpr Byte    kUnjustified   = 0xE0;
    static constexpr Byte    kCentered      = 0xE1;

    static constexpr Byte    kBoldOn        = 0x01;
    static constexpr Byte    kBoldOff       = 0x02;
    static constexpr Byte    kSuperOn       = 0x03;
    static constexpr Byte    kSubOff        = 0x06;
    static constexpr Byte    kUnderlineOn   = 0x07;
    static constexpr Byte    kUnderlineOff  = 0x08;
    static constexpr Byte    kPageNumber    = 0x09;
    static constexpr Byte    kStickySpace   = 0x0B;
    static constexpr Byte    kDate          = 0x0E;
    static constexpr Byte    kTime          = 0x0F;
    static constexpr Byte    kTab           = 0x16;
    static constexpr Byte    kTabFill       = 0x17;

};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksDb
//
//  An AppleWorks database file, $19, as File Type Note $19 sets it out: a
//  header with each category's name, 600 bytes for each report, then the
//  standard values and a record for each entry, to a length of $FFFF. A
//  date and a time are stored coded, and drawn as AppleWorks draws them.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWorksDb
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static std::wstring  DecodeEntry (std::span<const Byte> bytes);

private:
    static constexpr Byte    kFileType       = 0x19;
    static constexpr size_t  kCategoriesAt   = 35;
    static constexpr size_t  kReportsAt      = 38;
    static constexpr size_t  kNamesAt        = 357;
    static constexpr size_t  kNameBytes      = 22;
    static constexpr size_t  kReportBytes    = 600;
    static constexpr Byte    kDate           = 0xC0;
    static constexpr Byte    kTime           = 0xD4;
    static constexpr Byte    kEndOfRecord    = 0xFF;
    static constexpr Word    kEndOfFile      = 0xFFFF;
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksSs
//
//  An AppleWorks spreadsheet file, $1B, as File Type Note $1B sets it out:
//  a 300-byte header, then a record for each row with an entry, to a
//  length of $FFFF. Each cell is decoded as the file stores it: a label's
//  text, a value's number in its format, and a formula's last result.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWorksSs
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static std::wstring  GetColumnLetters (size_t column);
    static std::wstring  FormatNumber     (double value, int format, int decimals);

private:
    static constexpr Byte    kFileType       = 0x1B;
    static constexpr size_t  kHeaderBytes    = 300;
    static constexpr size_t  kWidthsAt       = 4;
    static constexpr size_t  kFormatAt       = 139;
    static constexpr size_t  kDecimalsAt     = 140;
    static constexpr size_t  kMinVersionAt   = 242;
    static constexpr Byte    kEndOfRow       = 0xFF;
    static constexpr Word    kEndOfFile      = 0xFFFF;

    static constexpr int     kFormatStandard = 1;
    static constexpr int     kFormatFixed    = 2;
    static constexpr int     kFormatDollars  = 3;
    static constexpr int     kFormatCommas   = 4;
    static constexpr int     kFormatPercent  = 5;

    static std::wstring  DecodeCell     (std::span<const Byte> cell, Byte width, int sheetFormat, int sheetDecimals);
    static std::wstring  AddThousands   (const std::wstring & digits);
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksGsWp
//
//  An AppleWorks GS word processor file, $50/$8010, as File Type Note
//  $50/$8010 sets it out: a header and globals, then the body's paragraph
//  array, rulers and text blocks, and the page header's and footer's after
//  them. A paragraph is a 7-byte header, then text with style codes inline,
//  to a carriage return.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWorksGsWp
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte    kFileType       = 0x50;
    static constexpr Word    kAuxType        = 0x8010;
    static constexpr size_t  kGlobalsAt      = 282;
    static constexpr size_t  kBodyVarsAt     = kGlobalsAt + 146;
    static constexpr size_t  kHeadVarsAt     = kGlobalsAt + 226;
    static constexpr size_t  kFootVarsAt     = kGlobalsAt + 306;
    static constexpr size_t  kLastParagraph  = 10;
    static constexpr size_t  kArrayAt        = kGlobalsAt + 386;
    static constexpr size_t  kEntryBytes     = 12;
    static constexpr size_t  kRulerBytes     = 52;
    static constexpr size_t  kHeaderBytes    = 7;

    static constexpr Byte    kFontChange     = 0x01;
    static constexpr Byte    kStyleChange    = 0x02;
    static constexpr Byte    kSizeChange     = 0x03;
    static constexpr Byte    kColorChange    = 0x04;
    static constexpr Byte    kPageNumber     = 0x05;
    static constexpr Byte    kDate           = 0x06;
    static constexpr Byte    kTime           = 0x07;
    static constexpr Byte    kTab            = 0x09;
    static constexpr Byte    kReturn         = 0x0D;

    static Alignment  GetAlignment     (Word statusBits);
    static void       DecodeParagraph  (std::span<const Byte> bytes, Charset charset, Paragraph & outParagraph);
};
