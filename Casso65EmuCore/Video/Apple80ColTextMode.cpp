#include "Pch.h"

#include "Apple80ColTextMode.h"
#include "CharacterRom.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple80ColTextMode
//
////////////////////////////////////////////////////////////////////////////////

Apple80ColTextMode::Apple80ColTextMode (MemoryBus & bus)
    : m_bus (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word Apple80ColTextMode::GetActivePageAddress (bool page2) const
{
    return page2 ? static_cast<Word> (0x0800) : static_cast<Word> (0x0400);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  80-column text mode renders 80x24 characters. Columns alternate between
//  aux and main memory (aux = even columns, main = odd columns).
//  For now, this renders similarly to 40-col but at half width per char.
//
////////////////////////////////////////////////////////////////////////////////

void Apple80ColTextMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{

    Word pageBase = GetActivePageAddress (m_page2);

    static constexpr int kTextCols   = 40;
    static constexpr int kTextRows   = 24;
    static constexpr int kCharWidth  = 7;
    static constexpr int kCharHeight = 8;

    static constexpr uint32_t kColorGreen = 0xFF00FF00;
    static constexpr uint32_t kColorBlack = 0xFF000000;

    // Render main memory characters (40 cols) as if 80-col
    // Proper aux RAM interleaving requires aux memory access
    for (int row = 0; row < kTextRows; row++)
    {
        Word rowAddr = static_cast<Word> (pageBase + 128 * (row % 8) + 40 * (row / 8));

        for (int col = 0; col < kTextCols; col++)
        {
            Byte charCode = (videoRam ? videoRam[static_cast<Word> (rowAddr + col)] : m_bus.ReadByte (static_cast<Word> (rowAddr + col)));

            Byte glyphIndex;

            if (charCode < 0x40)
            {
                glyphIndex = charCode;
            }
            else if (charCode < 0x80)
            {
                glyphIndex = static_cast<Byte> (charCode - 0x40);
            }
            else
            {
                glyphIndex = static_cast<Byte> (charCode - 0x80);
            }

            int romOffset = (glyphIndex >= 0x20 && glyphIndex <= 0x5F) ?
                (glyphIndex - 0x20) * 8 : 0;

            // Render at half-width (7 pixels per char, no 2x scaling)
            for (int py = 0; py < kCharHeight; py++)
            {
                Byte glyphRow = kApple2CharRom[romOffset + py];

                for (int px = 0; px < kCharWidth; px++)
                {
                    bool pixelOn = (glyphRow & (1 << px)) != 0;
                    uint32_t color = pixelOn ? kColorGreen : kColorBlack;

                    // Each char takes 7 pixels wide in 80-col mode
                    int fbX = col * kCharWidth * 2 + px;
                    int fbY = row * kCharHeight * 2 + py * 2;

                    if (fbX < fbWidth && fbY + 1 < fbHeight)
                    {
                        framebuffer[fbY * fbWidth + fbX]       = color;
                        framebuffer[(fbY + 1) * fbWidth + fbX] = color;
                    }
                }
            }
        }
    }
}
