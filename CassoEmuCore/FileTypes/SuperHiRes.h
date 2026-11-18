#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes
//
//  The IIgs's super hi-res pictures: a 32 KB screen dump, $C1/$0000; the
//  same packed with PackBytes, $C0/$0001; Apple Preferred, $C0/$0002, whose
//  lines are packed one by one; and Paintworks, $C0/$0000, 320-mode lines
//  under one palette. Each line has a scan-line control byte: 640 or 320
//  mode, fill mode, and its palette. A picture with any 640-mode line is
//  drawn 640 wide, a 320-mode pixel two wide; colors are RGB444, each
//  component times 17.
//
////////////////////////////////////////////////////////////////////////////////

class SuperHiRes
{
public:
    //  A picture to draw: each line's pixel bytes, its control byte, and the
    //  16 colors it uses.
    struct Screen
    {
        size_t                                bytesPerLine = 160;
        std::vector<Byte>                     pixels;
        std::vector<Byte>                     scbs;
        std::vector<std::array<uint32_t, 16>> palettes;     // by the control byte's palette number
        std::vector<std::array<uint32_t, 16>> linePalettes; // one a line, for 3200-color pictures
        size_t                                lines        = 200;
    };

    static bool     MatchesScreen     (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     MatchesPacked     (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     MatchesPaintworks (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  DecodeScreen      (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT  DecodePacked      (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT  DecodePaintworks  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    //  The screen layout's 32 KB: pixels, control bytes, palettes.
    static void      FromScreenBytes (std::span<const Byte> bytes, Screen & outScreen);
    static uint32_t  ToArgb          (Byte low, Byte high);
    static void      ReadPalette     (std::span<const Byte> bytes, std::array<uint32_t, 16> & outPalette, bool reversed = false);
    static void      Render          (const Screen & screen, DecodedFile & outFile);

    static constexpr size_t  kScreenBytes  = 32768;
    static constexpr size_t  kPixelBytes   = 32000;
    static constexpr size_t  kScbOffset    = 0x7D00;
    static constexpr size_t  kPaletteStart = 0x7E00;

private:
    static constexpr Byte    kTypePnt      = 0xC0;
    static constexpr Byte    kTypePic      = 0xC1;
    static constexpr Byte    kTypeBin      = 0x06;
    static constexpr Byte    kMode640      = 0x80;
    static constexpr Byte    kFill         = 0x20;

    static HRESULT  DecodePreferred (std::span<const Byte> bytes, DecodedFile & outFile, std::wstring & outError);
};





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200
//
//  3200-color pictures, a palette for each line: Brooks, $C1/$0002, the
//  pixels then each line's 16 colors in reverse order; `.3201` files, "APP"
//  and a NUL, the palettes, then PackBytes pixels; and DreamGrafix,
//  $C0/$8005, 12-bit LZW with a footer of its mode, height, width and
//  "DreamWorld".
//
////////////////////////////////////////////////////////////////////////////////

class SuperHiRes3200
{
public:
    static bool     MatchesBrooks      (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     Matches3201        (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     MatchesDreamGrafix (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  DecodeBrooks       (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT  Decode3201         (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT  DecodeDreamGrafix  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    //  DreamGrafix's LZW: codes of 9 to 12 bits, least significant first, 256
    //  clearing the table and 257 ending.
    static HRESULT  UnpackLzw (std::span<const Byte> in, size_t limit, std::vector<Byte> & outBytes);

private:
    static constexpr size_t  kBrooksBytes  = 38400;
    static constexpr size_t  kPaletteBytes = 6400;
    static constexpr size_t  kFooterBytes  = 17;
    static constexpr int     kClear        = 256;
    static constexpr int     kEnd          = 257;
    static constexpr int     kFirstFree    = 258;

    static void  DrawWithLinePalettes (std::span<const Byte> pixels, std::span<const Byte> palettes, DecodedFile & outFile);
};
