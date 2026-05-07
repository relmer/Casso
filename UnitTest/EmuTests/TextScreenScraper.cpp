#include "../CassoEmuCore/Pch.h"

#include "TextScreenScraper.h"


namespace
{
    static constexpr int   kRowGroupSize       = 8;
    static constexpr int   kRowGroupCount      = 3;
    static constexpr int   kRowsPerGroupBytes  = 0x80;
    static constexpr int   kSubRowStride       = 0x28;
    static constexpr Byte  kInverseUpper       = 0x40;
    static constexpr Byte  kPrintableMin       = 0x20;
    static constexpr Byte  kPrintableMax       = 0x7E;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowBaseAddress
//
//  Row Y (0..23) decomposes into (group=Y/8, sub=Y%8). Base address is
//  pageBase + sub*0x80 + group*0x28. This is the canonical Apple ][
//  interleave that maps 24 logical rows onto the 1 KiB text page with
//  64 unused "screen hole" bytes per group.
//
////////////////////////////////////////////////////////////////////////////////

Word TextScreenScraper::RowBaseAddress (Word pageBase, int row)
{
    int   group = row / kRowGroupSize;
    int   sub   = row % kRowGroupSize;

    return static_cast<Word> (pageBase + sub * kRowsPerGroupBytes + group * kSubRowStride);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Glyph
//
//  Maps a //e screen byte to a printable 7-bit character. Handles the
//  three character regions (inverse $00-$3F, flashing $40-$7F, normal
//  $80-$FF) by clearing bit 7 and folding the inverse-uppercase block
//  into ASCII uppercase. Non-printable results collapse to '.'.
//
////////////////////////////////////////////////////////////////////////////////

char TextScreenScraper::Glyph (Byte screenByte)
{
    Byte   ch;

    ch = static_cast<Byte> (screenByte & 0x7F);

    if (screenByte < kInverseUpper)
    {
        ch = static_cast<Byte> (ch | 0x40);
    }

    if (ch < kPrintableMin || ch > kPrintableMax)
    {
        return '.';
    }

    return static_cast<char> (ch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scrape40
//
//  40-column text scrape from a single contiguous main-RAM buffer. Each
//  row is decoded into a 40-character std::string of printable glyphs.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TextScreenScraper::Scrape40 (const Byte * mainRam, Word pageBase)
{
    std::vector<std::string>   rows;
    int                        row;
    int                        col;

    rows.reserve (kRows);

    for (row = 0; row < kRows; row++)
    {
        std::string   line;
        Word          base = RowBaseAddress (pageBase, row);

        line.resize (kCols40);

        for (col = 0; col < kCols40; col++)
        {
            line[col] = Glyph (mainRam[base + col]);
        }

        rows.push_back (std::move (line));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scrape80
//
//  80-column text scrape. Each logical 80-col character pair maps to a
//  single 40-col text-page address: aux[$0400+addr] supplies even-indexed
//  columns (0, 2, 4, ...) and main[$0400+addr] supplies odd-indexed
//  columns (1, 3, 5, ...). This is the same routing the //e Video-7 /
//  AppleColor //e firmware engages when PR#3 enables 80COL.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TextScreenScraper::Scrape80 (
    const Byte * mainRam,
    const Byte * auxRam,
    Word         pageBase)
{
    std::vector<std::string>   rows;
    int                        row;
    int                        cell;

    rows.reserve (kRows);

    for (row = 0; row < kRows; row++)
    {
        std::string   line;
        Word          base = RowBaseAddress (pageBase, row);

        line.resize (kCols80);

        for (cell = 0; cell < kCols40; cell++)
        {
            line[cell * 2]     = Glyph (auxRam [base + cell]);
            line[cell * 2 + 1] = Glyph (mainRam[base + cell]);
        }

        rows.push_back (std::move (line));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scrape
//
//  Scrapes the active text page, choosing 40 vs. 80 columns based on the
//  live AppleIIeSoftSwitchBank state and the active text page based on
//  PAGE2 (with 80STORE blocking PAGE2 from shifting the read window in
//  the 80-col case).
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TextScreenScraper::Scrape (const EmulatorCore & core)
{
    const Byte *   mainRam;
    const Byte *   auxRam;
    Word           pageBase;
    bool           col80;
    bool           page2;

    mainRam = core.cpu->GetMemory ();
    auxRam  = core.mmu->GetAuxBuffer ();

    col80    = core.softSwitches->Is80ColMode ();
    page2    = core.softSwitches->IsPage2     ();
    pageBase = (page2 && !col80) ? kTextPage2 : kTextPage1;

    if (col80)
    {
        return Scrape80 (mainRam, auxRam, pageBase);
    }

    return Scrape40 (mainRam, pageBase);
}
