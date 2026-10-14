#pragma once

#include "Pch.h"


class AppleTextFont;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile
//
//  A TrueType font made from the character set an AppleTextFont loaded, for
//  DirectWrite to lay out and select Apple text as it does any other.
//
//  EACH LIT DOT IS A SQUARE, a row's adjacent dots merged into one rectangle.
//  Eight rows fill the em and seven dots the advance, so at a size that is a
//  whole multiple of eight pixels every dot covers whole pixels and the text
//  is as sharp as the ROM's own. The baseline is one row above the bottom,
//  where the descenders of the //e's lowercase and the comma's tail end.
//
//  It is built in memory at run time from the user's own ROM, and written to
//  no file, so nothing of Apple's is redistributed. Printable ASCII is mapped;
//  anything else falls back to another font.
//
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class AppleFontFile
{
public:
    static constexpr const wchar_t *  kFamily = L"Casso Apple II";

    static constexpr wchar_t  kFirstChar   = L' ';
    static constexpr wchar_t  kLastChar    = L'~';
    static constexpr int      kUnitsPerDot = 125;
    static constexpr int      kUnitsPerEm  = 8 * kUnitsPerDot;
    static constexpr int      kAdvance     = 7 * kUnitsPerDot;
    static constexpr int      kAscent      = 7 * kUnitsPerDot;
    static constexpr int      kDescent     = kUnitsPerDot;

    static void  Build (const AppleTextFont & font, std::vector<Byte> & outFont);

private:
    struct Table
    {
        uint32_t           tag = 0;
        std::vector<Byte>  bytes;
    };

    static void      PutU16   (std::vector<Byte> & out, uint32_t value);
    static void      PutU32   (std::vector<Byte> & out, uint32_t value);
    static uint32_t  MakeTag  (const char (& name)[5]);
    static uint32_t  Checksum (const std::vector<Byte> & bytes, size_t offset, size_t length);

    static void  BuildGlyph (const AppleTextFont & font, wchar_t ch, std::vector<Byte> & outGlyph, int & outPoints, int & outContours, int & outLeft);
    static void  BuildHead  (std::vector<Byte> & out);
    static void  BuildHhea  (std::vector<Byte> & out, int glyphCount);
    static void  BuildMaxp  (std::vector<Byte> & out, int glyphCount, int maxPoints, int maxContours);
    static void  BuildCmap  (std::vector<Byte> & out);
    static void  BuildName  (std::vector<Byte> & out);
    static void  BuildOs2   (std::vector<Byte> & out);
    static void  BuildPost  (std::vector<Byte> & out);
    static void  Assemble   (std::vector<Table> & tables, std::vector<Byte> & outFont);
};
