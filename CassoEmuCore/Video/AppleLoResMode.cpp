#include "Pch.h"

#include "AppleLoResMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple II Lo-Res 16-Color Palette (RGBA)
//
////////////////////////////////////////////////////////////////////////////////

// Apple //e LoRes 16-color palette in B8G8R8A8 byte layout (matches the
// DXGI_FORMAT_B8G8R8A8_UNORM swap chain set up in D3DRenderer.cpp). Byte 0
// = B, byte 1 = G, byte 2 = R, byte 3 = A; the little-endian uint32_t hex
// literal therefore reads as 0xAARRGGBB. See Video/PixelFormat.h for the
// project-wide byte-order convention.
static const uint32_t kLoResColors[16] =
{
    0xFF000000,   //  0: Black       RGB(  0,  0,  0)
    0xFFDD2266,   //  1: Magenta     RGB(221, 34,102)
    0xFF000099,   //  2: Dark Blue   RGB(  0,  0,153)
    0xFFDD0044,   //  3: Purple      RGB(221,  0, 68)
    0xFF002200,   //  4: Dark Green  RGB(  0, 34,  0)
    0xFF555555,   //  5: Grey 1      RGB( 85, 85, 85)
    0xFF0022CC,   //  6: Medium Blue RGB(  0, 34,204)
    0xFF66AAFF,   //  7: Light Blue  RGB(102,170,255)
    0xFF885500,   //  8: Brown       RGB(136, 85,  0)
    0xFFFF4400,   //  9: Orange      RGB(255, 68,  0)
    0xFFAAAAAA,   // 10: Grey 2      RGB(170,170,170)
    0xFFFF8888,   // 11: Pink        RGB(255,136,136)
    0xFF00DD00,   // 12: Light Green RGB(  0,221,  0)
    0xFFFFFF00,   // 13: Yellow      RGB(255,255,  0)
    0xFF44FFDD,   // 14: Aquamarine  RGB( 68,255,221)
    0xFFFFFFFF,   // 15: White       RGB(255,255,255)
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
