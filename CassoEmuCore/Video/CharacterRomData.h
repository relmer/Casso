#pragma once

#include "Pch.h"


////////////////////////////////////////////////////////////////////////////////
//
//  CharacterRomData
//
//  Loads and decodes Apple II/II+/IIe character generator ROM files.
//  Provides a 256-glyph table for normal and (optionally) alt char sets.
//  Each glyph is 8 rows of 7 bits (bit 0 = leftmost dot).
//
//  Supports two file formats:
//    2KB (Apple II/II+): single 256-char set; chars 0x00-0x7F appear inverse,
//      0x40-0x7F also flash; 0x80-0xFF are normal. Bit ordering is reversed
//      in the source data (bit 6 = leftmost dot, needs reversal).
//    4KB (Apple //e enhanced): two 256-char sets (normal + alt/MouseText).
//      Chars 0x00-0x3F = inverse, 0x40-0x7F = flash, 0x80-0xFF = normal.
//      Source data is bit-inverted (XOR 0xFF) to flip lit dots.
//
////////////////////////////////////////////////////////////////////////////////

class CharacterRomData
{
public:
    static constexpr size_t kCharsPerSet  = 256;
    static constexpr size_t kBytesPerChar = 8;
    static constexpr size_t kSetSize      = kCharsPerSet * kBytesPerChar;  // 2048

    CharacterRomData ();

    // Load from file. Returns S_OK on success.
    HRESULT LoadFromFile (const string & filePath);

    // Load from a 2KB or 4KB in-memory buffer (test/headless path).
    HRESULT LoadFromMemory (const Byte * data, size_t size);

    // Load embedded fallback (96-char Apple II/II+ font for $20-$5F).
    void LoadEmbeddedFallback ();

    // Get a single row byte for a glyph. row is 0..7.
    // Returned byte: bit 0 = leftmost lit pixel.
    Byte GetGlyphRow (Byte glyphIndex, int row, bool altCharSet = false) const;

    bool HasAltCharSet () const { return m_hasAltCharSet; }

private:
    void Decode2K (const vector<Byte> & raw);
    void Decode4K (const vector<Byte> & raw);

    // [setIndex][char][row] -- setIndex: 0=normal, 1=alt
    Byte m_glyphs[2][kCharsPerSet][kBytesPerChar] = {};
    bool m_hasAltCharSet = false;
};
