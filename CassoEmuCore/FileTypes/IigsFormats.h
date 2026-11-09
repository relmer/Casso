#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IigsFont
//
//  A QuickDraw II font file, $C8/$0000, as File Type Note $C8 and the
//  QuickDraw II chapter of the Apple IIgs Toolbox Reference set it out: the
//  family name as a Pascal string, the IIgs header with its offset in words
//  to the Macintosh font record, then that record: the character range,
//  the strike's size, the strike a bit a pixel with the leftmost in the
//  high bit, and the location and offset/width tables.
//
////////////////////////////////////////////////////////////////////////////////

class IigsFont
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static std::wstring  DescribeStyle (Word style);

private:
    static constexpr Byte    kFileType     = 0xC8;
    static constexpr size_t  kRecordWords  = 13;
    static constexpr Word    kMissing      = 0xFFFF;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IigsIcons
//
//  A Finder icon file, $CA, as File Type Note $CA sets it out: a 26-byte
//  header, then icon entries to one of length 0, each the owner's path,
//  the name pattern, type and aux type, and a large and a small icon. An
//  icon is a type word, the image's size, height and width, the image at
//  4 bits a pixel and then its mask. Each entry is a row: the large icon,
//  its mask, the small icon and its mask, each line drawn twice, as the
//  Finder's 640 mode pixels are twice as tall as wide.
//
////////////////////////////////////////////////////////////////////////////////

class IigsIcons
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte      kFileType    = 0xCA;
    static constexpr size_t    kHeaderBytes = 26;
    static constexpr size_t    kNameAt      = 66;
    static constexpr size_t    kTypeAt      = 82;
    static constexpr size_t    kIconAt      = 86;
    static constexpr int       kGap         = 4;
    static constexpr uint32_t  kBackground  = 0xFF808080;
    static constexpr uint32_t  kMaskInk     = 0xFF000000;
    static constexpr uint32_t  kMaskPaper   = 0xFFFFFFFF;

    struct Icon
    {
        int                    width  = 0;
        int                    height = 0;
        std::span<const Byte>  image;
        std::span<const Byte>  mask;
    };

    static bool  ReadIcon  (std::span<const Byte> bytes, size_t at, Icon & outIcon, size_t & outEnd);
    static Byte  GetPixel  (std::span<const Byte> plane, const Icon & icon, int x, int y);
    static void  DrawIcon  (const Icon & icon, bool drawMask, int left, int top, DecodedFile & outFile);
};





////////////////////////////////////////////////////////////////////////////////
//
//  IigsAnimation
//
//  A Paintworks animation file, $C2/$0000: a super hi-res screen, the first
//  frame; then the data's length, the delay between frames, a flag word,
//  the animation's length, and entries of an offset and a value word, an
//  offset of 0 ending each frame.
//
////////////////////////////////////////////////////////////////////////////////

class IigsAnimation
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte    kFileType    = 0xC2;
    static constexpr size_t  kScreenBytes = 0x8000;
    static constexpr size_t  kRecordsAt   = 0x800C;
};
