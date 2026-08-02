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

// BGRA byte layout — see Video/PixelFormat.h.
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
    static_assert (kGridCols == kTextCols && kGridRows == kTextRows,
                   "dirty-row cache grid must match the render grid");

    // Flash phase is driven externally via SetFlashState (from emulated time),
    // not advanced here. Dirty-row rendering: redraw only the rows whose 80
    // effective char codes changed since the last render into THIS framebuffer
    // (plus, on a flash flip, rows holding a flashing glyph). A full redraw is
    // forced when reuse is unsafe -- first render, a different target buffer, a
    // change of aux pointer / charset / on-color. See AppleTextMode::Render.
    // 80-col always uses page 1.
    Word pageBase = static_cast<Word> (0x0400);

    bool full = !m_cacheValid
             || framebuffer  != m_prevFramebuffer
             || m_auxMem     != m_prevAuxMem
             || m_altCharSet != m_prevAltChar
             || m_onColor    != m_prevOnColor;

    bool flashFlip = m_flashOn != m_prevFlashOn;

    for (int row = 0; row < kTextRows; row++)
    {
        Word   rowAddr  = static_cast<Word> (pageBase + 128 * (row % 8) + 40 * (row / 8));
        Byte * cacheRow = &m_prevBytes[row * kTextCols];
        Byte   rowBytes[kTextCols];
        bool   changed  = false;

        for (int col = 0; col < kTextCols; col++)
        {
            int  memCol  = col / 2;
            bool fromAux = (col % 2) == 0;
            Word addr    = static_cast<Word> (rowAddr + memCol);
            Byte c       = 0;

            // Mirror RenderRowRange's read order exactly so the diff matches
            // what gets rasterized (aux even columns, main odd; bus fallback).
            if (fromAux && m_auxMem != nullptr) { c = m_auxMem[addr];       }
            else if (videoRam != nullptr)       { c = videoRam[addr];       }
            else                                { c = m_bus.ReadByte (addr); }

            rowBytes[col] = c;
            changed      |= (c != cacheRow[col]);
        }

        bool dirty = full || changed || (flashFlip && RowHasFlashChar (rowBytes));

        if (dirty)
        {
            RenderRowRange (row, row + 1, videoRam, framebuffer, fbWidth, fbHeight);
        }

        for (int col = 0; col < kTextCols; col++)
        {
            cacheRow[col] = rowBytes[col];
        }
    }

    m_prevFramebuffer = framebuffer;
    m_prevAuxMem      = m_auxMem;
    m_prevAltChar     = m_altCharSet;
    m_prevOnColor     = m_onColor;
    m_prevFlashOn     = m_flashOn;
    m_cacheValid      = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowHasFlashChar
//
//  As AppleTextMode::RowHasFlashChar, over the 80 interleaved char codes.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple80ColTextMode::RowHasFlashChar (const Byte * rowBytes) const
{
    if (m_altCharSet)
    {
        return false;
    }

    for (int col = 0; col < kTextCols; col++)
    {
        if (rowBytes[col] >= 0x40 && rowBytes[col] < 0x80)
        {
            return true;
        }
    }

    return false;
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
            //
            // The //e enhanced video ROM (Decode4K) stores the inverse
            // slots ($00-$3F, in BOTH primary and alt sets) already in
            // their inverse-rendered bitmap form (UTAIIe Tables
            // 8.2/8.3). The flash slots ($40-$7F primary) alias the
            // same bytes -- the toggle between inverse and normal phase
            // is what the renderer's XOR achieves. So:
            //
            //   - //e ROM, inverse slot:  display as-stored (no XOR).
            //   - //e ROM, flash slot:    XOR on the "normal" phase
            //     of the blink (so we toggle stored-inverse vs XORed-
            //     normal each ~16 frames).
            //   - ][/][+ ROM:             XOR on inverse OR flash phase
            //     (Decode2K stores normal-form bitmaps).
            //
            // Without the //e branch the cursor cell ($20 stored as a
            // pre-inverted solid block) would be XOR'd back to empty
            // -- the "missing 80-col cursor" symptom.
            bool isIIeRom = m_charRom.HasAltCharSet();
            bool inverse  = charCode < 0x40;
            bool flash    = !m_altCharSet && (charCode >= 0x40) && (charCode < 0x80);

            bool showInverse = (inverse && !isIIeRom) || (flash && m_flashOn);

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
                    uint32_t color   = pixelOn ? m_onColor : kColorBlack;

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
