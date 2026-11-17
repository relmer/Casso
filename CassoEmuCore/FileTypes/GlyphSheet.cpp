#include "Pch.h"

#include "FileTypes/GlyphSheet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GlyphSheet::Draw
//
//  A cell is the glyph's box and a one-pixel rule on its right and bottom.
//
////////////////////////////////////////////////////////////////////////////////

void GlyphSheet::Draw (DecodedFile & outFile) const
{
    int     cellWidth  = 1;
    int     cellHeight = 1;
    int     rows       = 0;



    for (const Glyph & glyph : glyphs)
    {
        cellWidth  = std::max (cellWidth,  glyph.width);
        cellHeight = std::max (cellHeight, glyph.height);
    }

    cellWidth++;
    cellHeight++;
    rows = std::max (1, ((int) glyphs.size() + kPerRow - 1) / kPerRow);

    outFile.kind   = DecodedFile::Kind::Picture;
    outFile.width  = cellWidth * kPerRow;
    outFile.height = cellHeight * rows;
    outFile.bgra.assign ((size_t) outFile.width * (size_t) outFile.height, kPaper);

    for (int y = 0; y < outFile.height; y++)
    {
        for (int x = 0; x < outFile.width; x++)
        {
            if ((x + 1) % cellWidth == 0 || (y + 1) % cellHeight == 0)
            {
                outFile.bgra[(size_t) y * (size_t) outFile.width + (size_t) x] = kRule;
            }
        }
    }

    for (size_t at = 0; at < glyphs.size(); at++)
    {
        const Glyph &  glyph = glyphs[at];
        int            left  = (int) (at % kPerRow) * cellWidth;
        int            top   = (int) (at / kPerRow) * cellHeight;

        for (int y = 0; y < glyph.height; y++)
        {
            for (int x = 0; x < glyph.width; x++)
            {
                if (glyph.pixels[(size_t) y * (size_t) glyph.width + (size_t) x])
                {
                    outFile.bgra[(size_t) (top + y) * (size_t) outFile.width + (size_t) (left + x)] = kInk;
                }
            }
        }
    }

    outFile.notes.push_back (std::format (L"{} {} {} {}", name, family, style, size > 0 ? std::to_wstring (size) : std::wstring()));
}
