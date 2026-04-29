#include "Pch.h"

#include "AppleTextMode.h"
#include "CharacterRom.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int kTextCols       = 40;
static constexpr int kTextRows       = 24;
static constexpr int kCharWidth      = 7;
static constexpr int kCharHeight     = 8;
static constexpr int kScaleX         = 2;   // Each pixel doubled horizontally
static constexpr int kScaleY         = 2;   // Each pixel doubled vertically

static constexpr uint32_t kColorGreen  = 0xFF00FF00;   // ARGB green
static constexpr uint32_t kColorBlack  = 0xFF000000;   // ARGB black
static constexpr uint32_t kColorWhite  = 0xFFFFFFFF;   // ARGB white





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextMode
//
////////////////////////////////////////////////////////////////////////////////

AppleTextMode::AppleTextMode (MemoryBus & bus)
    : m_bus        (bus),
      m_flashOn    (true),
      m_frameCount (0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowBaseAddress
//
//  Apple II text/lo-res interleaved row address calculation:
//    base + 128 * (row % 8) + 40 * (row / 8)
//
////////////////////////////////////////////////////////////////////////////////

Word AppleTextMode::RowBaseAddress (int row, Word pageBase)
{
    return static_cast<Word> (pageBase + 128 * (row % 8) + 40 * (row / 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word AppleTextMode::GetActivePageAddress (bool page2) const
{
    return page2 ? static_cast<Word> (0x0800) : static_cast<Word> (0x0400);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
////////////////////////////////////////////////////////////////////////////////

void AppleTextMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{
    UNREFERENCED_PARAMETER (videoRam);
    UNREFERENCED_PARAMETER (fbHeight);

    m_frameCount++;

    // Flash toggles every ~16 frames (approximately 0.5 second at 60fps)
    m_flashOn = ((m_frameCount / 16) & 1) == 0;

    Word pageBase = 0x0400;  // Default to page 1

    for (int row = 0; row < kTextRows; row++)
    {
        Word rowAddr = RowBaseAddress (row, pageBase);

        for (int col = 0; col < kTextCols; col++)
        {
            Byte charCode = m_bus.ReadByte (static_cast<Word> (rowAddr + col));

            // Decode character mode from high bits
            // $00-$3F: Inverse
            // $40-$7F: Flash
            // $80-$FF: Normal
            bool inverse = false;
            bool flash   = false;
            Byte glyphIndex;

            if (charCode < 0x40)
            {
                // Inverse: display character (charCode + $40) inverted
                inverse    = true;
                glyphIndex = charCode;
            }
            else if (charCode < 0x80)
            {
                // Flash: display character, alternating normal/inverse
                flash      = true;
                glyphIndex = static_cast<Byte> (charCode - 0x40);
            }
            else
            {
                // Normal
                glyphIndex = static_cast<Byte> (charCode - 0x80);
            }

            // Map glyph index to character ROM offset
            // Our ROM covers $20-$5F (space through underscore)
            int romOffset;

            if (glyphIndex >= 0x20 && glyphIndex <= 0x5F)
            {
                romOffset = (glyphIndex - 0x20) * 8;
            }
            else
            {
                romOffset = 0;  // Default to space
            }

            // Determine if we show normal or inverted
            bool showInverse = inverse || (flash && m_flashOn);

            // Render the 7x8 glyph scaled to 14x16 in the framebuffer
            for (int py = 0; py < kCharHeight; py++)
            {
                Byte glyphRow = kApple2CharRom[romOffset + py];

                for (int px = 0; px < kCharWidth; px++)
                {
                    bool pixelOn = (glyphRow & (1 << px)) != 0;

                    if (showInverse)
                    {
                        pixelOn = !pixelOn;
                    }

                    uint32_t color = pixelOn ? kColorGreen : kColorBlack;

                    // Scale 2x in both directions
                    int fbX = (col * kCharWidth + px) * kScaleX;
                    int fbY = (row * kCharHeight + py) * kScaleY;

                    for (int sy = 0; sy < kScaleY; sy++)
                    {
                        for (int sx = 0; sx < kScaleX; sx++)
                        {
                            int idx = (fbY + sy) * fbWidth + (fbX + sx);

                            if (fbX + sx < fbWidth && fbY + sy < fbHeight)
                            {
                                framebuffer[idx] = color;
                            }
                        }
                    }
                }
            }
        }
    }
}
