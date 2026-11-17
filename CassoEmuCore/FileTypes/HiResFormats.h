#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"
#include "FileTypes/GlyphSheet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Lz4fh
//
//  fhpack's LZ4FH, $08/$8066: the magic byte $66, then chunks of a literal
//  and match length in one byte, the literals, and a match offset from the
//  start of the output. A match length byte of 253 is no match and 254 the
//  end. It unpacks to an 8 KB hi-res page.
//
////////////////////////////////////////////////////////////////////////////////

class Lz4fh
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Unpack  (std::span<const Byte> bytes, std::vector<Byte> & outPage);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte  kMagic   = 0x66;
    static constexpr Byte  kNoMatch = 253;
    static constexpr Byte  kEnd     = 254;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HiResFont
//
//  An Apple II hi-res font, 96 glyphs of 7 by 8 pixels, a byte a row with
//  the leftmost pixel in bit 0; or an Apple /// font of 128 glyphs, whose
//  rows' high bits are for inverse mode.
//
////////////////////////////////////////////////////////////////////////////////

class HiResFont
{
public:
    static bool     MatchesApple2 (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     MatchesApple3 (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode        (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr size_t  kApple2Bytes = 768;
    static constexpr size_t  kApple3Bytes = 1024;
    static constexpr Byte    kTypeFont    = 0x07;
    static constexpr Byte    kTypeBin     = 0x06;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PrintShopGraphic
//
//  Print Shop clip art, 88 by 52 pixels, the leftmost in the high bit, a
//  set bit black: a binary file at $4800, $5800, $6800 or $7800 of 572
//  bytes, or 576 with a 4-byte header; Print Shop GS's monochrome $F8/$C313;
//  and its color $F8/$C323, a yellow, a magenta and a cyan plane. Drawn as
//  printed, twice as wide and three times as tall.
//
////////////////////////////////////////////////////////////////////////////////

class PrintShopGraphic
{
public:
    static bool     Matches     (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     MatchesGs   (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode      (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr size_t  kPlaneBytes = 572;
    static constexpr int     kWidth      = 88;
    static constexpr int     kHeight     = 52;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PrintShopFont
//
//  A Print Shop font: an optional 12-byte Print Shop Companion header, then
//  59 widths, 59 heights, and the low and high bytes of 59 pointers into
//  the glyphs at $6000, each glyph a bit a pixel, leftmost first. Glyph 32
//  is the current graphic, not in the file.
//
////////////////////////////////////////////////////////////////////////////////

class PrintShopFont
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr size_t  kGlyphs = 59;
    static constexpr Word    kBase   = 0x6000;

    static bool  TryRead (std::span<const Byte> bytes, size_t header, GlyphSheet & outSheet);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ShapeTable
//
//  An Applesoft shape table: a shape count, a spare byte, an offset to each
//  shape, and each shape's bytes of three moves, A and B with plot bits and
//  C without, ending in a zero byte. Each shape is drawn at scale 1 and
//  rotation 0, eight to a row, its number under it, clipped to its cell.
//
////////////////////////////////////////////////////////////////////////////////

class ShapeTable
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     Check   (std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr int  kPerRow    = 8;
    static constexpr int  kCell      = 64;
    static constexpr int  kLabel     = 8;
    static constexpr int  kMaxMoves  = 4096;

    static void  DrawNumber (DecodedFile & file, int left, int top, int number);
};





////////////////////////////////////////////////////////////////////////////////
//
//  FontrixFont
//
//  A Fontrix font: a DOS 3.3 binary file whose name starts SET., or a
//  ProDOS BIN at $6400, with $90 $F7 $B2 at $18, or its inverse in a font
//  saved while editing. $10 is the glyph count, $11 the first character,
//  $12 set for a proportional font and $14 the height; from $20, a file
//  offset for each character from the space, each glyph's rows a whole
//  number of bytes with the leftmost pixel in bit 0.
//
////////////////////////////////////////////////////////////////////////////////

class FontrixFont
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr size_t  kMaxBytes  = 20480;
    static constexpr size_t  kTableAt   = 0x20;
    static constexpr Word    kAuxType   = 0x6400;
};
