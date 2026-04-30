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
//  ScanlineAddress
//
//  Hi-res scanline interleaving (same as text but with 8 sub-rows):
//  base + 128*(row%8) + 40*(row/64) + 1024*((row/8)%8)
//  Where row is 0-191 scanlines
//
////////////////////////////////////////////////////////////////////////////////

Word AppleHiResMode::ScanlineAddress (int scanline, Word pageBase)
{
    int group   = scanline / 64;   // 0-2 (which group of 64 scanlines)
    int subRow  = scanline % 8;    // 0-7 (which of 8 interleave rows)
    int lineInGroup = (scanline % 64) / 8;  // 0-7 (which line within group)

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
////////////////////////////////////////////////////////////////////////////////

void AppleHiResMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{

    Word pageBase = GetActivePageAddress (m_page2);

    for (int scanline = 0; scanline < 192; scanline++)
    {
        Word lineAddr = ScanlineAddress (scanline, pageBase);

        bool prevPixelOn = false;

        for (int byteIdx = 0; byteIdx < 40; byteIdx++)
        {
            Byte data = (videoRam ? videoRam[static_cast<Word> (lineAddr + byteIdx)] : m_bus.ReadByte (static_cast<Word> (lineAddr + byteIdx)));

            bool paletteBit = (data & 0x80) != 0;

            // 7 pixels per byte (bits 0-6, LSB = leftmost)
            for (int bit = 0; bit < 7; bit++)
            {
                bool pixelOn = (data & (1 << bit)) != 0;
                int screenCol = byteIdx * 7 + bit;

                uint32_t color;

                if (!pixelOn)
                {
                    color = kNtscBlack;
                }
                else if (prevPixelOn || (bit < 6 && (data & (1 << (bit + 1)))))
                {
                    // Adjacent ON pixels = white
                    color = kNtscWhite;

                    // Also make previous pixel white if it was a non-white color
                    if (prevPixelOn && screenCol > 0)
                    {
                        int prevFbX = (screenCol - 1) * 2;
                        int fbY     = scanline * 2;

                        if (prevFbX >= 0 && prevFbX + 1 < fbWidth)
                        {
                            framebuffer[fbY * fbWidth + prevFbX]     = kNtscWhite;
                            framebuffer[fbY * fbWidth + prevFbX + 1] = kNtscWhite;

                            if (fbY + 1 < fbHeight)
                            {
                                framebuffer[(fbY + 1) * fbWidth + prevFbX]     = kNtscWhite;
                                framebuffer[(fbY + 1) * fbWidth + prevFbX + 1] = kNtscWhite;
                            }
                        }
                    }
                }
                else
                {
                    // Single pixel ON — color depends on column and palette
                    bool evenCol = (screenCol % 2) == 0;

                    if (!paletteBit)
                    {
                        color = evenCol ? kNtscViolet : kNtscGreen;
                    }
                    else
                    {
                        color = evenCol ? kNtscBlue : kNtscOrange;
                    }
                }

                // Write 2x2 pixels to framebuffer (560x384 from 280x192)
                int fbX = screenCol * 2;
                int fbY = scanline * 2;

                if (fbX + 1 < fbWidth && fbY + 1 < fbHeight)
                {
                    framebuffer[fbY * fbWidth + fbX]           = color;
                    framebuffer[fbY * fbWidth + fbX + 1]       = color;
                    framebuffer[(fbY + 1) * fbWidth + fbX]     = color;
                    framebuffer[(fbY + 1) * fbWidth + fbX + 1] = color;
                }

                prevPixelOn = pixelOn;
            }
        }
    }
}
