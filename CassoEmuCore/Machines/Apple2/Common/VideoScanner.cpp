#include "Pch.h"

#include "Machines/Apple2/Common/VideoScanner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VideoScanner::GetScannedAddress
//
//  The vertical counter reads $100-$1FF from line 0 and wraps to $FA-$FF
//  for the frame's last lines. The horizontal counter's preset state and
//  its first counting state both read 0, so the first address of each
//  horizontal blank is scanned twice.
//
////////////////////////////////////////////////////////////////////////////////

Word VideoScanner::GetScannedAddress (uint32_t cycleInFrame, const VideoScanMode & mode)
{
    uint32_t  line      = (cycleInFrame / kCyclesPerScanline) % kScanlinesPerFrame;
    uint32_t  cycle     = cycleInFrame % kCyclesPerScanline;
    uint32_t  h         = (cycle == 0) ? 0 : cycle - 1;
    uint32_t  v         = (line < 256) ? 0x100 + line : 0xFA + (line - 256);
    uint32_t  vabc      = v & 0x07;
    uint32_t  row       = (v >> 3) & 0x3F;
    uint32_t  v3        = (row >> 3) & 1;
    uint32_t  v4        = (row >> 4) & 1;
    uint32_t  sum       = ((h >> 3) + 0x0D + ((v4 << 3) | (v3 << 2) | (v4 << 1) | v3)) & 0x0F;
    bool      page2     = mode.page2 && !mode.store80;
    bool      mixedText = mode.mixed && (row & 0x14) == 0x14;
    bool      hires     = mode.graphics && mode.hires && !mixedText;
    uint32_t  address   = (h & 0x07) | (sum << 3) | ((row & 0x07) << 7);



    if (hires)
    {
        address |= (vabc << 10) | (page2 ? 0x4000u : 0x2000u);
    }
    else
    {
        address |= page2 ? 0x0800u : 0x0400u;
    }

    return (Word) address;
}
