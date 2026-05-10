#include "Pch.h"

#include "CharacterRomData.h"
#include "CharacterRom.h"
#include "Ehm.h"


static constexpr size_t k2KBytes = 2048;
static constexpr size_t k4KBytes = 4096;


////////////////////////////////////////////////////////////////////////////////
//
//  CharacterRomData
//
////////////////////////////////////////////////////////////////////////////////

CharacterRomData::CharacterRomData ()
{
    LoadEmbeddedFallback();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFromFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CharacterRomData::LoadFromFile (const string & filePath)
{
    HRESULT       hr      = S_OK;
    ifstream      file (filePath, ios::binary | ios::ate);
    bool          fileOk  = false;
    auto          rawSize = streamsize {0};
    vector<Byte>  raw;



    fileOk = file.good();
    CBRA (fileOk);

    rawSize = file.tellg();
    file.seekg (0, ios::beg);

    CBRA (rawSize == k2KBytes || rawSize == k4KBytes);

    raw.resize (static_cast<size_t> (rawSize));
    file.read (reinterpret_cast<char *> (raw.data()), rawSize);

    CBRA (file.gcount() == rawSize);

    if (rawSize == k2KBytes)
    {
        Decode2K (raw);
    }
    else
    {
        Decode4K (raw);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFromMemory
//
//  Decode a 2KB or 4KB raw video ROM image already resident in memory.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CharacterRomData::LoadFromMemory (const Byte * data, size_t size)
{
    HRESULT      hr = S_OK;
    vector<Byte> raw;



    CBRA (data != nullptr);
    CBRA (size == k2KBytes || size == k4KBytes);

    raw.assign (data, data + size);

    if (size == k2KBytes)
    {
        Decode2K (raw);
    }
    else
    {
        Decode4K (raw);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadEmbeddedFallback
//
//  Use the embedded 96-char (chars $20-$5F) glyph table. Other characters
//  are zero-filled (display as blank space).
//
////////////////////////////////////////////////////////////////////////////////

void CharacterRomData::LoadEmbeddedFallback ()
{
    memset (m_glyphs, 0, sizeof (m_glyphs));
    m_hasAltCharSet = false;

    // The embedded ROM covers glyph indices $20-$5F (96 chars).
    // The full 256-char table is built so all three character regions
    // (inverse $00-$3F, flash $40-$7F, normal $80-$FF) display the same
    // ASCII characters. Inversion happens at render time.
    for (int glyph = 0x20; glyph <= 0x5F; glyph++)
    {
        int srcOffset = (glyph - 0x20) * static_cast<int> (kBytesPerChar);

        for (int row = 0; row < static_cast<int> (kBytesPerChar); row++)
        {
            Byte data = kApple2CharRom[srcOffset + row];

            // Inverse region $00-$3F: maps to chars $00-$3F (control chars
            // displayed inverse). We store the ASCII shape; render inverts.
            if (glyph - 0x40 >= 0)
            {
                m_glyphs[0][glyph - 0x40][row] = data;
            }

            // Flash region $40-$7F: chars $40-$7F (uppercase ASCII)
            m_glyphs[0][glyph][row] = data;

            // Normal region $80-$FF: chars $80-$FF (high bit set ASCII)
            m_glyphs[0][glyph + 0x80][row] = data;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetGlyphRow
//
////////////////////////////////////////////////////////////////////////////////

Byte CharacterRomData::GetGlyphRow (Byte glyphIndex, int row, bool altCharSet) const
{
    if (row < 0 || row >= static_cast<int> (kBytesPerChar))
    {
        return 0;
    }

    int set = (altCharSet && m_hasAltCharSet) ? 1 : 0;
    return m_glyphs[set][glyphIndex][row];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Decode2K
//
//  Apple II/II+ 2KB video ROM. The ROM has bits in reversed order
//  (bit 6 = leftmost dot) and dots are inverted (1 = unlit). The low
//  1KB contains chars 0x00-0x7F; setting bit 7 of the source byte means
//  "do not invert", otherwise the glyph is inverted (UTAII:8-30/8-31).
//  We store the "lit dots = 1" form normalized so render code is uniform.
//
////////////////////////////////////////////////////////////////////////////////

void CharacterRomData::Decode2K (const vector<Byte> & raw)
{
    memset (m_glyphs, 0, sizeof (m_glyphs));
    m_hasAltCharSet = false;

    for (int i = 0; i < static_cast<int> (kCharsPerSet); i++)
    {
        int RA = i * static_cast<int> (kBytesPerChar);

        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            Byte n = raw[RA + y];

            // For chars in low 1KB ($00-$7F): if bit 7 is clear, invert (xor with 0x7F)
            // This produces the "lit dot" pattern.
            if (RA < 1024)
            {
                if (!(n & 0x80))
                {
                    n = n ^ 0x7F;
                }
            }

            // Reverse bits 0..6 (bit 6 of source becomes bit 0 of output)
            Byte d = 0;
            for (int j = 0; j < 7; j++)
            {
                if (n & (1 << j))
                {
                    d |= (1 << (6 - j));
                }
            }

            m_glyphs[0][i][y] = d;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Decode4K
//
//  Apple //e enhanced 4KB video ROM. Two 2KB halves: primary char set + alt.
//  Source bytes are bit-inverted (XOR 0xFF) to flip lit dots. No bit reversal
//  needed -- bit 0 is already leftmost (different chip from II/II+).
//
////////////////////////////////////////////////////////////////////////////////

void CharacterRomData::Decode4K (const vector<Byte> & raw)
{
    memset (m_glyphs, 0, sizeof (m_glyphs));
    m_hasAltCharSet = true;

    // Primary char set (first 2KB):
    //
    //   chars [0x00..0x3F] inverse / [0x40..0x7F] flash:
    //     RA = 0..0x1FF (loop i=0..63, RA+=8). Both i and i+64 map to same data.
    //   chars [0x80..0xFF] normal:
    //     [0x80..0xBF] from RA = 0x400 + (i-128)*8 = 0x400..0x5FF
    //     [0xC0..0xFF] from RA = 0x600 + (i-192)*8 = 0x600..0x7FF
    //
    int RA = 0;

    for (int i = 0; i < 64; i++, RA += 8)
    {
        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            Byte d = raw[RA + y] ^ 0xFF;
            m_glyphs[0][i]      [y] = d;
            m_glyphs[0][i + 64] [y] = d;
        }
    }

    RA = 0x400;
    for (int i = 128; i < 192; i++, RA += 8)
    {
        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            m_glyphs[0][i][y] = raw[RA + y] ^ 0xFF;
        }
    }

    RA = 0x600;
    for (int i = 192; i < 256; i++, RA += 8)
    {
        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            m_glyphs[0][i][y] = raw[RA + y] ^ 0xFF;
        }
    }

    // Alternate char set (second 2KB): straightforward 0x800..0xFFF -> chars 0..255
    RA = 0x800;
    for (int i = 0; i < static_cast<int> (kCharsPerSet); i++, RA += 8)
    {
        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            m_glyphs[1][i][y] = raw[RA + y] ^ 0xFF;
        }
    }
}
