#include "Pch.h"

#include "AppleHiResMode.h"
#include "NtscColorTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleHiResMode
//
////////////////////////////////////////////////////////////////////////////////

AppleHiResMode::AppleHiResMode (MemoryBus & bus)
    : m_bus (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetScanlineAddress
//
//  Hi-res scanline interleaving (same as text but with 8 sub-rows):
//  base + 128*(row%8) + 40*(row/64) + 1024*((row/8)%8)
//  Where row is 0-191 scanlines
//
////////////////////////////////////////////////////////////////////////////////

Word AppleHiResMode::GetScanlineAddress (int scanline, Word pageBase)
{
    int  group       = scanline / 64;   // 0-2 (which group of 64 scanlines)
    int  subRow      = scanline % 8;   // 0-7 (which of 8 interleave rows)
    int  lineInGroup = (scanline % 64) / 8;   // 0-7 (which line within group)



    return static_cast<Word> (
        pageBase + subRow * 1024 + group * 40 + lineInGroup * 128);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word AppleHiResMode::GetActivePageAddress (bool page2) const
{
    return page2 ? static_cast<Word> (0x4000) : static_cast<Word> (0x2000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Rasterizes the hi-res screen. Which decode runs depends on the monitor:
//  a color monitor gets NTSC artifact color at 280 pixels, a monochrome one
//  gets the 560 half-dot stream. They are separate decodes because artifact
//  color is what a color monitor MAKES from the dots -- tinting it cannot
//  recover the dots, and a monochrome monitor never saw the color at all.
//
//  The description below is the color path.
//
//  TWO PASSES per scanline, and the split is what makes artifact color
//  possible. A hi-res pixel's color depends on its NEIGHBORS -- the Apple II
//  produced color by exploiting NTSC chroma aliasing, so an isolated dot is
//  colored while adjacent dots read as white. A single pass cannot know what
//  is to the right yet, so pass 1 decodes the whole line's pixels and palette
//  bits and pass 2 colors them with both neighbors in hand.
//
//  Bit 7 of each byte is a PALETTE selector, not pixel data -- it shifts that
//  byte's seven pixels by half a dot and swaps the color pair. Which is why
//  the palette bit is captured per pixel rather than per byte: a run can cross
//  a byte boundary into a different palette.
//
//  Seven pixels per byte, forty bytes per line: the hardware packs 280 pixels
//  across, and the leftover eighth bit is the palette selector.
//
//  Scanlines are doubled vertically, so 192 emulated lines fill the 384-line
//  framebuffer and hi-res matches the other modes' geometry.
//
//  A null videoRam reads through the bus, honoring //e MMU banking; an
//  explicit pointer is the standalone path.
//
////////////////////////////////////////////////////////////////////////////////

void AppleHiResMode::Render (
    const Byte * videoRam,
    uint32_t   * framebuffer,
    int          fbWidth,
    int          fbHeight)
{
    static constexpr int kPixelsPerScanline   = 280;
    static constexpr int kHalfDotsPerScanline = 560;



    Word      pageBase      = GetActivePageAddress (m_page2);
    bool      pixels[280]   = {};
    bool      palettes[280] = {};
    bool      halfDots[560] = {};
    Byte      data          = 0;
    bool      palBit        = false;
    uint32_t  color         = 0;
    int       fbX           = 0;
    int       fbY           = 0;
    bool      leftOn        = false;
    bool      rightOn       = false;



    for (int scanline = 0; scanline < 192; scanline++)
    {
        Word lineAddr = GetScanlineAddress (scanline, pageBase);

        // Pass 1: decode all 280 pixels and palette bits
        for (int byteIdx = 0; byteIdx < 40; byteIdx++)
        {
            data   = videoRam
                   ? videoRam[static_cast<Word> (lineAddr + byteIdx)]
                   : m_bus.ReadByte (static_cast<Word> (lineAddr + byteIdx));
            palBit = (data & 0x80) != 0;

            for (int bit = 0; bit < 7; bit++)
            {
                int x = byteIdx * 7 + bit;

                pixels[x]   = (data & (1 << bit)) != 0;
                palettes[x] = palBit;
            }
        }

        fbY = scanline * 2;

        if (m_monochrome)
        {
            // Pass 2, monochrome: the 560 half-dot stream itself.
            //
            // The shift register clocks 7 pixels per byte, and the palette
            // bit DELAYS that byte's output by one half-dot -- which is the
            // physical cause of the color pair swap the color decode models
            // as a palette. A monochrome monitor has no color to swap, so
            // what it shows instead is the shift: a byte with bit 7 set
            // paints half a dot to the right of where its neighbor would.
            // That is real horizontal detail, and it only exists at 560.
            //
            // Each pixel occupies two adjacent half-dots. Runs are OR-ed in
            // rather than assigned, because a shifted byte butting against
            // an unshifted one makes the two overlap by a half-dot -- the
            // hardware lights that slot, so the later byte must not clear
            // what the earlier one lit.
            for (fbX = 0; fbX < kHalfDotsPerScanline; fbX++)
            {
                halfDots[fbX] = false;
            }

            for (int x = 0; x < kPixelsPerScanline; x++)
            {
                if (!pixels[x])
                {
                    continue;
                }

                int slot = x * 2 + (palettes[x] ? 1 : 0);

                if (slot < kHalfDotsPerScanline)
                {
                    halfDots[slot] = true;
                }

                if (slot + 1 < kHalfDotsPerScanline)
                {
                    halfDots[slot + 1] = true;
                }
            }

            for (fbX = 0; fbX < kHalfDotsPerScanline; fbX++)
            {
                color = halfDots[fbX] ? kNtscWhite : kNtscBlack;

                if (fbX < fbWidth && fbY + 1 < fbHeight)
                {
                    framebuffer[fbY       * fbWidth + fbX] = color;
                    framebuffer[(fbY + 1) * fbWidth + fbX] = color;
                }
            }

            continue;
        }

        // Pass 2, color: determine artifact color for each pixel
        for (int x = 0; x < 280; x++)
        {
            if (!pixels[x])
            {
                color = kNtscBlack;
            }
            else
            {
                leftOn  = (x > 0)   && pixels[x - 1];
                rightOn = (x < 279) && pixels[x + 1];

                if (leftOn || rightOn)
                {
                    color = kNtscWhite;
                }
                else
                {
                    // Isolated pixel — artifact color
                    bool evenCol = (x % 2) == 0;

                    if (!palettes[x])
                    {
                        color = evenCol ? kNtscViolet : kNtscGreen;
                    }
                    else
                    {
                        color = evenCol ? kNtscBlue : kNtscOrange;
                    }
                }
            }

            // Write 2x2 pixels to framebuffer (560x384 from 280x192)
            fbX = x * 2;

            framebuffer[fbY       * fbWidth + fbX]     = color;
            framebuffer[fbY       * fbWidth + fbX + 1] = color;
            framebuffer[(fbY + 1) * fbWidth + fbX]     = color;
            framebuffer[(fbY + 1) * fbWidth + fbX + 1] = color;
        }
    }
}
