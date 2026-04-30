#include "Pch.h"

#include "AppleDoubleHiResMode.h"
#include "AppleHiResMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDoubleHiResMode
//
//  560x192 double hi-res using interleaved aux/main memory.
//  Renders as monochrome hi-res fallback until aux memory is wired.
//
////////////////////////////////////////////////////////////////////////////////

AppleDoubleHiResMode::AppleDoubleHiResMode (MemoryBus & bus)
    : m_bus (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word AppleDoubleHiResMode::GetActivePageAddress (bool page2) const
{
    return page2 ? static_cast<Word> (0x4000) : static_cast<Word> (0x2000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Fallback: renders as monochrome hi-res until double hi-res is fully wired
//  via AuxRamCard. This prevents a black screen.
//
////////////////////////////////////////////////////////////////////////////////

void AppleDoubleHiResMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{

    Word pageBase = GetActivePageAddress (m_page2);

    for (int scanline = 0; scanline < 192; scanline++)
    {
        Word lineAddr = AppleHiResMode::ScanlineAddress (scanline, pageBase);

        for (int byteIdx = 0; byteIdx < 40; byteIdx++)
        {
            Byte data = (videoRam ? videoRam[static_cast<Word> (lineAddr + byteIdx)] : m_bus.ReadByte (static_cast<Word> (lineAddr + byteIdx)));

            for (int bit = 0; bit < 7; bit++)
            {
                bool pixelOn = (data & (1 << bit)) != 0;
                int screenCol = byteIdx * 7 + bit;

                uint32_t color = pixelOn ? 0xFFFFFFFF : 0xFF000000;

                int fbX = screenCol * 2;
                int fbY = scanline * 2;

                if (fbX + 1 < fbWidth && fbY + 1 < fbHeight)
                {
                    framebuffer[fbY * fbWidth + fbX]           = color;
                    framebuffer[fbY * fbWidth + fbX + 1]       = color;
                    framebuffer[(fbY + 1) * fbWidth + fbX]     = color;
                    framebuffer[(fbY + 1) * fbWidth + fbX + 1] = color;
                }
            }
        }
    }
}