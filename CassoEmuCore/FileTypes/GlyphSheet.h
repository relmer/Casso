#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GlyphSheet
//
//  A font's glyphs drawn sixteen to a row, black on white, each in a cell
//  as wide as the widest glyph and as tall as the tallest, with a gray line
//  between cells. A missing glyph's cell is left blank. The font's name,
//  family, style and size go in the decoded file's notes.
//
////////////////////////////////////////////////////////////////////////////////

struct Glyph
{
    int                width  = 0;
    int                height = 0;
    std::vector<bool>  pixels;      // row by row, set for ink
};



class GlyphSheet
{
public:
    std::wstring        name;
    std::wstring        family;
    std::wstring        style;
    int                 size = 0;
    std::vector<Glyph>  glyphs;

    void  Draw (DecodedFile & outFile) const;

private:
    static constexpr int       kPerRow = 16;
    static constexpr uint32_t  kPaper  = 0xFFFFFFFF;
    static constexpr uint32_t  kInk    = 0xFF000000;
    static constexpr uint32_t  kRule   = 0xFFC0C0C0;
};
