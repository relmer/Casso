#include "Pch.h"

#include "Apple80ColTextMode.h"
#include "CharacterRomData.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple80ColTextMode
//
////////////////////////////////////////////////////////////////////////////////

// Singleton default char ROM (embedded fallback) for legacy single-arg constructor
static const CharacterRomData & GetDefaultCharRom()
{
    static CharacterRomData s_defaultRom;
    return s_defaultRom;
}

Apple80ColTextMode::Apple80ColTextMode (MemoryBus & bus)
    : m_bus     (bus),
      m_charRom (GetDefaultCharRom())
{
}

Apple80ColTextMode::Apple80ColTextMode (MemoryBus & bus, const CharacterRomData & charRom)
    : m_bus     (bus),
      m_charRom (charRom)
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
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int kTextCols   = 80;
static constexpr int kTextRows   = 24;
static constexpr int kCharWidth  = 7;
static constexpr int kCharHeight = 8;

static constexpr uint32_t kColorGreen = 0xFF00FF00;
static constexpr uint32_t kColorBlack = 0xFF000000;





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  80-column text mode renders 80x24 characters. Columns alternate between
//  aux and main memory: aux RAM provides even screen columns (0,2,4,...);
//  main RAM provides odd screen columns (1,3,5,...). Each character cell is
//  7 dots wide × 8 lines tall, doubled vertically to fit 384 framebuffer
//  rows. ALTCHARSET (audit M13) and FLASH (audit M14) are honored via the
//  same RenderRowRange helper used by the mixed-mode composed path.
//
////////////////////////////////////////////////////////////////////////////////

void Apple80ColTextMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{
    m_frameCount++;

    // Flash toggles every ~16 frames (approximately 0.5 second at 60fps).
    // ALTCHARSET=1 disables flash on the //e: only ALTCHARSET=0 mode flashes.
    m_flashOn = ((m_frameCount / 16) & 1) == 0;

    RenderRowRange (0, kTextRows, videoRam, framebuffer, fbWidth, fbHeight);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderRowRange
//
//  Shared render helper. Honors ALTCHARSET (audit M13) and FLASH (audit M14).
//  Caller controls flash state (m_flashOn) — only Render() advances it.
//
////////////////////////////////////////////////////////////////////////////////

void Apple80ColTextMode::RenderRowRange (
    int          startRow,
    int          endRow,
    const Byte * videoRam,
    uint32_t   * framebuffer,
    int          fbWidth,
    int          fbHeight)
{
    Word pageBase = static_cast<Word> (0x0400);   // 80-col always uses page 1

    for (int row = startRow; row < endRow; row++)
    {
        Word rowAddr = static_cast<Word> (pageBase + 128 * (row % 8) + 40 * (row / 8));

        for (int col = 0; col < kTextCols; col++)
        {
            int  memCol  = col / 2;
            bool fromAux = (col % 2) == 0;
            Word addr    = static_cast<Word> (rowAddr + memCol);

            Byte charCode = 0;

            if (fromAux && m_auxMem != nullptr)
            {
                charCode = m_auxMem[addr];
            }
            else if (videoRam != nullptr)
            {
                charCode = videoRam[addr];
            }
            else
            {
                charCode = m_bus.ReadByte (addr);
            }

            // Decode character mode from the top two bits.
            // ALTCHARSET=0:  $00-$3F inverse, $40-$7F flash, $80-$FF normal.
            // ALTCHARSET=1:  $00-$3F inverse, $40-$7F MouseText (alt set, no flash),
            //                $80-$FF normal.
            bool inverse = charCode < 0x40;
            bool flash   = !m_altCharSet && (charCode >= 0x40) && (charCode < 0x80);

            bool showInverse = inverse || (flash && m_flashOn);

            for (int py = 0; py < kCharHeight; py++)
            {
                Byte glyphRow = m_charRom.GetGlyphRow (charCode, py, m_altCharSet);

                if (showInverse)
                {
                    glyphRow = static_cast<Byte> (~glyphRow);
                }

                for (int px = 0; px < kCharWidth; px++)
                {
                    bool     pixelOn = (glyphRow & (1 << px)) != 0;
                    uint32_t color   = pixelOn ? kColorGreen : kColorBlack;

                    int fbX = col * kCharWidth + px;
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
