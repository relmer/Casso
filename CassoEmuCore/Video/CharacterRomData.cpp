#include "Pch.h"

#include "CharacterRomData.h"
#include "CharacterRom.h"


static constexpr size_t k2KBytes = 2048;
static constexpr size_t k4KBytes = 4096;





////////////////////////////////////////////////////////////////////////////////
//
//  CharacterRomData
//
////////////////////////////////////////////////////////////////////////////////

CharacterRomData::CharacterRomData()
{
    LoadEmbeddedFallback();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFromFile
//
//  Loads a character generator ROM, accepting the two sizes that exist.
//
//  The SIZE selects the decoder, and the two are genuinely different layouts:
//  a 2 KB ROM is the original single character set, while a 4 KB ROM carries
//  the //e's primary and alternate sets and needs its own unpacking. Size is
//  the only thing distinguishing them -- these files carry no header.
//
//  Any other size is REJECTED rather than partially decoded. A ROM of
//  unexpected length is not a character generator, and decoding it anyway
//  produces a screen of garbage glyphs that looks like a video bug rather than
//  a bad file.
//
//  The read is verified against the reported size, so a truncated file fails
//  here instead of leaving half the glyphs blank.
//
//  Everything asserts: the caller falls back to the embedded font, so a bad
//  ROM is a developer-visible problem rather than a user-facing failure.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CharacterRomData::LoadFromFile (const string & filePath)
{
    HRESULT       hr     = S_OK;
    bool          fileOk = false;
    vector<Byte>  raw;
    ifstream      file (filePath, ios::binary | ios::ate);
    auto             rawSize   = streamsize {0};
    std::streamsize  bytesRead = 0;



    fileOk = file.good();
    CBRA (fileOk);

    rawSize = file.tellg();
    file.seekg (0, ios::beg);

    CBRA (rawSize == k2KBytes || rawSize == k4KBytes);

    raw.resize (static_cast<size_t> (rawSize));
    file.read (reinterpret_cast<char *> (raw.data()), rawSize);

    bytesRead = file.gcount();
    CBRA (bytesRead == rawSize);

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

void CharacterRomData::LoadEmbeddedFallback()
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
    // A ROM without an alternate set falls back to set 0, so a //e-only
    // ALTCHARSET request on a ][ ROM renders the primary glyph rather than
    // indexing past the end.
    bool  inRange = (row >= 0 && row < static_cast<int> (kBytesPerChar));
    int   set     = (altCharSet && m_hasAltCharSet) ? 1 : 0;

    return inRange ? m_glyphs[set][glyphIndex][row] : (Byte) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Decode2K
//
//  Apple II/II+ 2KB video ROM. The ROM stores 256 cells of 8 rows; the
//  bits are in reversed order (bit 6 = leftmost dot). The low seven bits
//  carry the glyph's "lit dots = 1" pattern and are identical across all
//  four 64-char ranges ($00-$3F inverse, $40-$7F flash, $80-$BF and
//  $C0-$FF normal) -- bit 7 is only a range marker, NOT a per-glyph
//  invert flag. So every cell decodes to the same normal-form glyph and
//  the renderer applies the inverse ($00-$3F) / flash ($40-$7F) video at
//  draw time (UTAII:8-30/8-31).
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

            // Reverse bits 0..6 (bit 6 of source becomes bit 0 of output);
            // bit 7 is the range marker and is intentionally dropped.
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
    int RA = 0;



    memset (m_glyphs, 0, sizeof (m_glyphs));
    m_hasAltCharSet = true;

    // //e Enhanced video ROM (4 KB / 2732) decoder. Layout per UTAIIe
    // ch. 8 (Sather), Tables 8.2 / 8.3:
    //
    // ROM addressing: [RA10, RA9, RA8..RA3, RA2..RA0] where RA10/RA9
    // are derived from the video character byte (VID7/VID6) and the
    // ALTCHARSET / FLASH state.
    //
    // Decoded into two flat 256-char tables:
    //
    // PRIMARY set (ALTCHARSET=0):
    //   [00..3F] inverse + [40..7F] flash share offsets 0..0x1FF
    //     (RA10=0, RA9=0, FLASH overlay chooses inverse vs flash)
    //   [80..BF] normal lowercase from offsets 0x400..0x5FF
    //     (RA10=1, RA9=0)
    //   [C0..FF] normal uppercase from offsets 0x600..0x7FF
    //     (RA10=1, RA9=1)
    //
    // ALT set (ALTCHARSET=1):
    //   [00..FF] all read linearly from offsets 0..0x7FF
    //     (RA10=VID7, RA9=VID6 -- so chars $00-$3F are MouseText
    //     replacing the inverse glyphs, $40-$7F are the unused flash
    //     positions, $80-$FF are the same normal text glyphs as
    //     primary)
    //
    // Bytes in the ROM file have lit dots stored as 0; the renderer
    // wants 1=lit, hence the XOR with 0xFF.

    for (int i = 0; i < 64; i++, RA += 8)
    {
        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            Byte d = static_cast<Byte> (raw[RA + y] ^ 0xFF);
            m_glyphs[0][i]      [y] = d;
            m_glyphs[0][i + 64] [y] = d;
        }
    }

    RA = 0x400;
    for (int i = 128; i < 192; i++, RA += 8)
    {
        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            m_glyphs[0][i][y] = static_cast<Byte> (raw[RA + y] ^ 0xFF);
        }
    }

    RA = 0x600;
    for (int i = 192; i < 256; i++, RA += 8)
    {
        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            m_glyphs[0][i][y] = static_cast<Byte> (raw[RA + y] ^ 0xFF);
        }
    }

    // Alt set: all 256 chars read linearly from the first 2 KB.
    RA = 0;
    for (int i = 0; i < static_cast<int> (kCharsPerSet); i++, RA += 8)
    {
        for (int y = 0; y < static_cast<int> (kBytesPerChar); y++)
        {
            m_glyphs[1][i][y] = static_cast<Byte> (raw[RA + y] ^ 0xFF);
        }
    }
}
