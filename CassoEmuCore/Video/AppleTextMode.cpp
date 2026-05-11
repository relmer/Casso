#include "Pch.h"

#include "AppleTextMode.h"
#include "CharacterRomData.h"





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

// Singleton default char ROM (embedded fallback) for legacy single-arg constructor
static const CharacterRomData & GetDefaultCharRom()
{
    static CharacterRomData s_defaultRom;
    return s_defaultRom;
}

AppleTextMode::AppleTextMode (MemoryBus & bus)
    : m_bus     (bus),
      m_charRom (GetDefaultCharRom())
{
}

AppleTextMode::AppleTextMode (MemoryBus & bus, const CharacterRomData & charRom)
    : m_bus     (bus),
      m_charRom (charRom)
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
    m_frameCount++;

    // Flash toggles every ~16 frames (approximately 0.5 second at 60fps)
    m_flashOn = ((m_frameCount / 16) & 1) == 0;

    RenderRowRange (0, kTextRows, videoRam, framebuffer, fbWidth, fbHeight);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderRowRange
//
//  Renders rows [startRow, endRow) into framebuffer. Shared between full
//  Render() and the composed mixed-mode bottom-rows path (FR-017a / FR-020).
//  Does not advance the flash frame counter — caller controls that.
//
////////////////////////////////////////////////////////////////////////////////

void AppleTextMode::RenderRowRange (
    int          startRow,
    int          endRow,
    const Byte * videoRam,
    uint32_t   * framebuffer,
    int          fbWidth,
    int          fbHeight)
{
    UNREFERENCED_PARAMETER (fbHeight);

    Word pageBase   = GetActivePageAddress (m_page2);
    int  charStride = kCharWidth * kScaleX;

    for (int row = startRow; row < endRow; row++)
    {
        Word rowAddr     = RowBaseAddress (row, pageBase);
        int  fbRowOrigin = row * kCharHeight * kScaleY * fbWidth;

        for (int col = 0; col < kTextCols; col++)
        {
            Word addr     = static_cast<Word> (rowAddr + col);
            Byte charCode = videoRam ? videoRam[addr] : m_bus.ReadByte (addr);

            // Decode character mode from high bits
            // $00-$3F: Inverse
            // $40-$7F: Flash
            // $80-$FF: Normal
            bool inverse = false;
            bool flash   = false;

            if (charCode < 0x40)
            {
                inverse = true;
            }
            else if (charCode < 0x80)
            {
                flash = true;
            }

            bool showInverse = inverse || (flash && m_flashOn);
            int  fbColOrigin = fbRowOrigin + col * charStride;

            // Render the 7x8 glyph scaled 2x to 14x16 in the framebuffer.
            for (int py = 0; py < kCharHeight; py++)
            {
                Byte       glyphRow = m_charRom.GetGlyphRow (charCode, py, m_altCharSet);
                uint32_t * row0     = framebuffer + fbColOrigin;
                uint32_t * row1     = row0 + fbWidth;

                if (showInverse)
                {
                    glyphRow = static_cast<Byte> (~glyphRow);
                }

                for (int px = 0; px < kCharWidth; px++)
                {
                    uint32_t color = (glyphRow & (1 << px)) ? kColorGreen
                                                            : kColorBlack;

                    // Write 2x2 scaled pixel directly
                    row0[0] = color;
                    row0[1] = color;
                    row1[0] = color;
                    row1[1] = color;

                    row0 += kScaleX;
                    row1 += kScaleX;
                }

                fbColOrigin += fbWidth * kScaleY;
            }
        }
    }
}
