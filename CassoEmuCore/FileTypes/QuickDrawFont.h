#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawFont
//
//  A QuickDraw II bitmap font, in a $C8 font file's layout: the family name
//  as a Pascal string, the IIgs header (the offset in words to the font
//  record, the family number, the style and the size), then the Macintosh
//  font record. The record has the character range, the widest advance,
//  the kern, the ascent and descent, and the strike's row length. The strike
//  is a bit a pixel, the leftmost pixel in the high bit, and is followed by
//  the location table and the offset/width table. An offset/width entry's
//  high byte is the image's offset from the pen plus the kern, its low byte
//  the advance, and $FFFF a missing character, which is drawn as the
//  character after the last.
//
////////////////////////////////////////////////////////////////////////////////

class QuickDrawFont
{
public:
    static bool  TryParse (std::span<const Byte> bytes, QuickDrawFont & outFont);

    Word  GetFamily  () const { return m_family; }
    Word  GetStyle   () const { return m_style; }
    int   GetSize    () const { return m_size; }
    int   GetAscent  () const { return m_ascent; }
    int   GetDescent () const { return m_descent; }
    int   GetWidMax  () const { return m_widMax; }

    //  A character's advance, its image's width and its image's offset from
    //  the pen; false for a character the font doesn't have and no missing
    //  symbol either.
    bool  TryGetChar (Byte character, int & outAdvance, int & outWidth, int & outOffset) const;

    //  Whether a pixel of a character's image is set, x from the image's
    //  left edge and y from the top of the font's rectangle.
    bool  IsSet      (Byte character, int x, int y) const;

private:
    static constexpr size_t  kRecordWords = 13;
    static constexpr Word    kMissing     = 0xFFFF;

    bool  TryGetIndex (Byte character, size_t & outIndex) const;

    Word               m_family   = 0;
    Word               m_style    = 0;
    int                m_size     = 0;
    int                m_first    = 0;
    int                m_last     = 0;
    int                m_widMax   = 0;
    int                m_kernMax  = 0;
    int                m_ascent   = 0;
    int                m_descent  = 0;
    int                m_height   = 0;
    size_t             m_rowBytes = 0;
    std::vector<Byte>  m_strike;
    std::vector<Word>  m_locations;
    std::vector<Word>  m_offsetWidths;
};
