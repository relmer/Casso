#include "Pch.h"

#include "AppleLoResMode.h"
#include "NtscColorTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple II Lo-Res color
//
//  A nibble per block, straight into the machine's sixteen colors. Those
//  live in Video/NtscColorTable.h; lo-res names no colors of its own.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLoResMode
//
////////////////////////////////////////////////////////////////////////////////

AppleLoResMode::AppleLoResMode (MemoryBus & bus)
    : m_bus (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRowBaseAddress
//
////////////////////////////////////////////////////////////////////////////////

Word AppleLoResMode::GetRowBaseAddress (int row, Word pageBase)
{
    return static_cast<Word> (pageBase + 128 * (row % 8) + 40 * (row / 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word AppleLoResMode::GetActivePageAddress (bool page2) const
{
    return page2 ? static_cast<Word> (0x0800) : static_cast<Word> (0x0400);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Rasterizes the 40x48 lo-res screen from the SAME memory the text screen
//  uses.
//
//  That sharing is the hardware's design, not a shortcut: lo-res and text
//  occupy identical addresses and differ only in interpretation. So the row
//  addressing is the text layout -- 24 rows of 40 bytes -- and each byte
//  supplies TWO stacked blocks, low nybble on top and high nybble below,
//  giving 48 rows out of 24.
//
//  Block dimensions are derived from the framebuffer size rather than
//  hard-coded, so the same code fills whatever raster it is handed.
//
//  Bounds are checked per pixel because the derived block size truncates: a
//  framebuffer whose dimensions are not exact multiples of 40 and 48 leaves a
//  remainder, and writing it would run off the edge.
//
//  A null videoRam reads through the bus, honoring //e MMU banking; an
//  explicit pointer is the standalone path.
//
////////////////////////////////////////////////////////////////////////////////

void AppleLoResMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{

    Word pageBase = GetActivePageAddress (m_page2);

    // Lo-res is 40x48 blocks. Each byte has two 4-bit colors:
    // low nybble = top block, high nybble = bottom block.
    // Text memory is 24 rows of 40 bytes, each byte covers 2 lo-res rows.

    int blockW = fbWidth / 40;    // 14 pixels per block
    int blockH = fbHeight / 48;   // 8 pixels per block

    for (int textRow = 0; textRow < 24; textRow++)
    {
        Word rowAddr = GetRowBaseAddress (textRow, pageBase);

        for (int col = 0; col < 40; col++)
        {
            Byte data = (videoRam ? videoRam[static_cast<Word> (rowAddr + col)] : m_bus.ReadByte (static_cast<Word> (rowAddr + col)));

            uint32_t topColor    = kAppleColors[data & 0x0F];
            uint32_t bottomColor = kAppleColors[(data >> 4) & 0x0F];

            int loResRow1 = textRow * 2;
            int loResRow2 = textRow * 2 + 1;

            // Top block
            for (int py = 0; py < blockH; py++)
            {
                for (int px = 0; px < blockW; px++)
                {
                    int fbX = col * blockW + px;
                    int fbY = loResRow1 * blockH + py;

                    if (fbX < fbWidth && fbY < fbHeight)
                    {
                        framebuffer[fbY * fbWidth + fbX] = topColor;
                    }
                }
            }

            // Bottom block
            for (int py = 0; py < blockH; py++)
            {
                for (int px = 0; px < blockW; px++)
                {
                    int fbX = col * blockW + px;
                    int fbY = loResRow2 * blockH + py;

                    if (fbX < fbWidth && fbY < fbHeight)
                    {
                        framebuffer[fbY * fbWidth + fbX] = bottomColor;
                    }
                }
            }
        }
    }
}
