#include "Pch.h"

#include "Devices/RamDevice.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "TextScreenScraper.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetRowBaseAddress
//
//  Row Y (0..23) decomposes into (group=Y/8, sub=Y%8). Base address is
//  pageBase + sub*0x80 + group*0x28. This is the canonical Apple ][
//  interleave that maps 24 logical rows onto the 1 KiB text page with
//  64 unused "screen hole" bytes per group.
//
////////////////////////////////////////////////////////////////////////////////

Word TextScreenScraper::GetRowBaseAddress (Word pageBase, int row)
{
    constexpr int  kRowGroupSize      = 8;
    constexpr int  kRowsPerGroupBytes = 0x80;
    constexpr int  kSubRowStride      = 0x28;



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
    constexpr Byte  kInverseUpper = 0x40;
    constexpr Byte  kPrintableMin = 0x20;
    constexpr Byte  kPrintableMax = 0x7E;



    Byte   ch = static_cast<Byte> (screenByte & 0x7F);

    // The inverse block $00-$3F contains uppercase at ASCII minus $40, so
    // setting bit 6 folds it back.
    if (screenByte < kInverseUpper)
    {
        ch = static_cast<Byte> (ch | 0x40);
    }

    return (ch < kPrintableMin || ch > kPrintableMax) ? '.' : static_cast<char> (ch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scrape40
//
//  40-column text scrape of the page at `pageBase`, read through the bus
//  with the CPU's current banking. The page is copied through the bus and
//  decoded by the buffer overload.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TextScreenScraper::Scrape40 (MemoryBus & bus, Word pageBase)
{
    std::vector<Byte>   page (kPageBytes);
    int                 offset;



    for (offset = 0; offset < kPageBytes; offset++)
    {
        page[offset] = bus.ReadByte (static_cast<Word> (pageBase + offset));
    }

    return Scrape40 (page.data());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scrape40
//
//  40-column text scrape of one 1 KiB text page. Each row is decoded into
//  a 40-character std::string of printable glyphs.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TextScreenScraper::Scrape40 (const Byte * page)
{
    std::vector<std::string>   rows;
    int                        row;
    int                        col;



    rows.reserve (kRows);

    for (row = 0; row < kRows; row++)
    {
        std::string   line;
        Word          base = GetRowBaseAddress (0, row);

        line.resize (kCols40);

        for (col = 0; col < kCols40; col++)
        {
            line[col] = Glyph (page[base + col]);
        }

        rows.push_back (std::move (line));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scrape80
//
//  80-column text scrape. Each 40-column page offset supplies two screen
//  columns: the aux page byte is the even column (0, 2, 4, ...) and the
//  main page byte at the same offset is the odd column (1, 3, 5, ...).
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TextScreenScraper::Scrape80 (const Byte * mainPage, const Byte * auxPage)
{
    std::vector<std::string>   rows;
    int                        row;
    int                        cell;



    rows.reserve (kRows);

    for (row = 0; row < kRows; row++)
    {
        std::string   line;
        Word          base = GetRowBaseAddress (0, row);

        line.resize (kCols80);

        for (cell = 0; cell < kCols40; cell++)
        {
            line[cell * 2]     = Glyph (auxPage[base + cell]);
            line[cell * 2 + 1] = Glyph (mainPage[base + cell]);
        }

        rows.push_back (std::move (line));
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scrape
//
//  Scrapes the displayed text page, choosing 40 vs. 80 columns from 80COL
//  and the page from PAGE2. Under 80STORE, PAGE2 selects the aux bank for
//  CPU access rather than page 2 for display, matching VideoScanner.
//
//  A //e or //c screen is read from the main and aux RAM buffers, not
//  through the bus. The bus follows the CPU's banking: with RAMRD on, or
//  80STORE and PAGE2 on (the 80-column firmware sets PAGE2 around every
//  even-column write), a bus read of the text page returns aux, and the odd
//  columns come back as copies of the even ones.
//
//  A ][ or ][+ has no //e soft-switch bank and no MMU, so its screen is 40
//  columns of main memory, which nothing banks away from the bus.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TextScreenScraper::Scrape (MachineHost & host)
{
    MachineRefs           &  refs     = host.GetRefs();
    Apple2eSoftSwitchBank *  iieSw    = refs.iieSoftSwitches;
    Apple2eMmu            *  mmu      = host.GetMmu();
    const Byte            *  mainRam  = (mmu != nullptr && refs.mainRamDev != nullptr)
                                        ? refs.mainRamDev->GetData()
                                        : nullptr;
    bool                     col80    = iieSw != nullptr && iieSw->Is80ColMode();
    bool                     store80  = iieSw != nullptr && iieSw->Is80Store();
    bool                     page2    = refs.softSwitches != nullptr && refs.softSwitches->IsPage2();
    Word                     pageBase = (page2 && !store80) ? kTextPage2 : kTextPage1;



    if (mainRam == nullptr)
    {
        return Scrape40 (host.GetMemoryBus(), pageBase);
    }

    if (col80)
    {
        return Scrape80 (mainRam + pageBase, mmu->GetAuxBuffer() + pageBase);
    }

    return Scrape40 (mainRam + pageBase);
}
