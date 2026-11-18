#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IGlyphSource
//
//  A character generator's glyphs: 128 characters of 8 rows. Bits 0-6 of a
//  row are its dots, bit 0 leftmost; bit 7 is the row's flash select.
//
////////////////////////////////////////////////////////////////////////////////

class IGlyphSource
{
public:
    static constexpr int  kCharacterCount = 128;
    static constexpr int  kRowCount       = 8;

    virtual         ~IGlyphSource() = default;

    virtual Byte     GetGlyphRow (Byte code, int row) const = 0;
};
