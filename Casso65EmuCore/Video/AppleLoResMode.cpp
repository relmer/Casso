#include "Pch.h"

#include "AppleLoResMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple II Lo-Res 16-Color Palette (RGBA)
//
////////////////////////////////////////////////////////////////////////////////

static const uint32_t kLoResColors[16] =
{
    0xFF000000,   //  0: Black
    0xFF0000DD,   //  1: Magenta (Deep Red)
    0xFF000099,   //  2: Dark Blue
    0xFF4400DD,   //  3: Purple (Violet)
    0xFF002200,   //  4: Dark Green
    0xFF555555,   //  5: Grey 1
    0xFFCC2200,   //  6: Medium Blue
    0xFFFF4499,   //  7: Light Blue
    0xFF000066,   //  8: Brown
    0xFF0044FF,   //  9: Orange
    0xFFAAAAAA,   // 10: Grey 2
    0xFF8888FF,   // 11: Pink
    0xFF00DD00,   // 12: Light Green (Green)
    0xFF00FFFF,   // 13: Yellow
    0xFFDDFF44,   // 14: Aquamarine (Aqua)
    0xFFFFFFFF,   // 15: White
};





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
//  RowBaseAddress
//
////////////////////////////////////////////////////////////////////////////////

Word AppleLoResMode::RowBaseAddress (int row, Word pageBase)
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
        Word rowAddr = RowBaseAddress (textRow, pageBase);

        for (int col = 0; col < 40; col++)
        {
            Byte data = (videoRam ? videoRam[static_cast<Word> (rowAddr + col)] : m_bus.ReadByte (static_cast<Word> (rowAddr + col)));

            uint32_t topColor    = kLoResColors[data & 0x0F];
            uint32_t bottomColor = kLoResColors[(data >> 4) & 0x0F];

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
