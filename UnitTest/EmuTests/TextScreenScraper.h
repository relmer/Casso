#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextScreenScraper
//
//  Phase 7 (T067). Converts the //e text-page memory plus the live
//  80COL/PAGE2/MIXED state into a row-major std::vector<std::string>
//  for assertion. Implements the canonical Apple ][ interleaved text-
//  page address arithmetic for $0400-$07BF (24 visible rows of 40 chars)
//  and routes column 2N from aux memory and column 2N+1 from main memory
//  when 80COL is active. Test-screen contents are sanitized to printable
//  7-bit ASCII so assertions can use ordinary string compares (FR-016,
//  FR-017, FR-037).
//
////////////////////////////////////////////////////////////////////////////////

class TextScreenScraper
{
public:
    static constexpr int    kRows         = 24;
    static constexpr int    kCols40       = 40;
    static constexpr int    kCols80       = 80;
    static constexpr Word   kTextPage1    = 0x0400;
    static constexpr Word   kTextPage2    = 0x0800;
    static constexpr int    kPageBytes    = 0x0400;

    // Row-major scrape of the displayed text screen. The number of columns
    // in each row matches the displayed mode (40 or 80).
    static std::vector<std::string>  Scrape   (MachineHost & host);

    // 40 columns read through the CPU's view of memory. On a //e or //c that
    // view can be aux, and an 80-column screen comes back as its odd columns
    // only; Scrape reads the RAM the video scanner reads.
    static std::vector<std::string>  Scrape40 (MemoryBus & bus, Word pageBase);

    // Each page pointer addresses the first byte of a 1 KiB text page.
    static std::vector<std::string>  Scrape40 (const Byte * page);
    static std::vector<std::string>  Scrape80 (const Byte * mainPage, const Byte * auxPage);

    // Address of the first byte of `row` in a 40-column text page based
    // at `pageBase` ($0400 or $0800). Standard Apple ][ interleave.
    static Word  GetRowBaseAddress (Word pageBase, int row);

    // Convert a screen byte (which may be inverse, flashing, or normal
    // ASCII with bit-7 set) to a printable 7-bit character.
    static char  Glyph (Byte screenByte);
};
