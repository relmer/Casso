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

static constexpr uint32_t kColorGreen  = 0xFF00FF00;   // BGRA green
static constexpr uint32_t kColorBlack  = 0xFF000000;   // BGRA black
static constexpr uint32_t kColorWhite  = 0xFFFFFFFF;   // BGRA white





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





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextMode::AppleTextMode
//
////////////////////////////////////////////////////////////////////////////////

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
//  Rasterizes the 40-column text screen, redrawing only the rows that changed.
//
//  The dirty-row cache works because the framebuffer PERSISTS between frames,
//  so a row whose bytes are unchanged still holds a correct image and can be
//  skipped. At a BASIC prompt that reduces a full 40x24 raster to nothing.
//
//  A full redraw is forced whenever the cache cannot be trusted -- the first
//  render, a DIFFERENT target framebuffer (the shell reuses one, tests may
//  not), or a change to page, character set, or on-color, each of which
//  re-shapes every row rather than any particular one.
//
//  A flash-phase flip redraws only the rows that actually contain a flashing
//  glyph, since the rest are unaffected by the phase.
//
//  Flash state is pushed in from emulated time rather than advanced here,
//  which is what lets a steady screen skip re-rasterizing without freezing the
//  cursor blink -- the two would otherwise be the same decision.
//
//  Row bytes are read into a local buffer BEFORE the compare, so the cache
//  update is one pass and the same bytes drive both the dirty test and the
//  stored copy.
//
//  A null videoRam reads through the bus instead, which is how the //e's MMU
//  banking is honored; passing an explicit pointer is the standalone path.
//
////////////////////////////////////////////////////////////////////////////////

void AppleTextMode::Render (
    const Byte * videoRam,
    uint32_t * framebuffer,
    int fbWidth,
    int fbHeight)
{
    Word  pageBase  = 0;
    bool  flashFlip = false;



    static_assert (kGridCols == kTextCols && kGridRows == kTextRows,
                   "dirty-row cache grid must match the render grid");

    // Flash phase is driven externally via SetFlashState (from emulated time),
    // not advanced here -- so a steady screen can skip re-rasterizing without
    // freezing the cursor/flash blink.
    //
    // Dirty-row rendering: the framebuffer persists between frames, so redraw
    // only the rows whose text bytes changed since the last Render() into THIS
    // framebuffer. A full redraw is forced when the cache can't be trusted --
    // first render, a different target framebuffer (the shell always reuses
    // one; tests may not), or a change to page / charset / on-color (which
    // re-shapes every row). On a flash-phase flip only the rows that actually
    // contain a flashing glyph need redrawing.
    pageBase = GetActivePageAddress (m_page2);

    bool full = !m_cacheValid
             || framebuffer  != m_prevFramebuffer
             || m_page2      != m_prevPage2
             || m_altCharSet != m_prevAltChar
             || m_onColor    != m_prevOnColor;

    flashFlip = m_flashOn != m_prevFlashOn;

    for (int row = 0; row < kTextRows; row++)
    {
        Word    rowAddr             = RowBaseAddress (row, pageBase);
        Byte  * cacheRow            = &m_prevBytes[row * kTextCols];
        Byte    rowBytes[kTextCols];
        bool    changed             = false;
        bool    dirty               = false;

        for (int col = 0; col < kTextCols; col++)
        {
            Word addr    = static_cast<Word> (rowAddr + col);
            Byte b       = videoRam ? videoRam[addr] : m_bus.ReadByte (addr);
            rowBytes[col] = b;
            changed      |= (b != cacheRow[col]);
        }

        dirty = full || changed || (flashFlip && RowHasFlashChar (rowBytes));

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
    m_prevPage2       = m_page2;
    m_prevAltChar     = m_altCharSet;
    m_prevOnColor     = m_onColor;
    m_prevFlashOn     = m_flashOn;
    m_cacheValid      = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowHasFlashChar
//
//  A glyph flashes with the flash clock only for char codes $40-$7F, and only
//  when ALTCHARSET is off (the //e enhanced ROM remaps that range to MouseText
//  under ALTCHARSET, which does not blink). Inverse ($00-$3F) and normal
//  ($80-$FF) glyphs are unaffected by the flash phase.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleTextMode::RowHasFlashChar (const Byte * rowBytes) const
{
    int   col      = 0;
    bool  hasFlash = false;



    // ALTCHARSET remaps $40-$7F to MouseText, which does not blink, so the
    // whole row is non-flashing without inspecting a single byte.
    for (col = 0; !m_altCharSet && !hasFlash && col < kTextCols; col++)
    {
        hasFlash = (rowBytes[col] >= 0x40 && rowBytes[col] < 0x80);
    }

    return hasFlash;
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
    Word  pageBase   = 0;
    int   charStride = 0;



    UNREFERENCED_PARAMETER (fbHeight);

    pageBase = GetActivePageAddress (m_page2);
    charStride = kCharWidth * kScaleX;

    for (int row = startRow; row < endRow; row++)
    {
        Word rowAddr     = RowBaseAddress (row, pageBase);
        int  fbRowOrigin = row * kCharHeight * kScaleY * fbWidth;

        for (int col = 0; col < kTextCols; col++)
        {
            Word  addr        = static_cast<Word> (rowAddr + col);
            Byte  charCode    = videoRam ? videoRam[addr] : m_bus.ReadByte (addr);
            bool  showInverse = false;
            int   fbColOrigin = 0;

            // Decode character mode from high bits
            // $00-$3F: Inverse
            // $40-$7F: Flash (suppressed when ALTCHARSET=1 on //e)
            // $80-$FF: Normal
            //
            // The //e enhanced video ROM (Decode4K) stores inverse
            // slots ($00-$3F) already in their inverse-rendered
            // bitmap form (UTAIIe 8.2/8.3); the renderer must display
            // them as-stored. ][/][+ (Decode2K) stores normal-form
            // bitmaps and the renderer must XOR for inverse display.
            // Flash slots ($40-$7F primary) alias the inverse bytes
            // on //e -- the renderer's XOR is what flips between the
            // stored-inverse and XORed-normal phase each blink.
            bool isIIeRom = m_charRom.HasAltCharSet();
            bool inverse  = false;
            bool flash    = false;

            if (charCode < 0x40)
            {
                inverse = true;
            }
            else if (charCode < 0x80)
            {
                flash = !m_altCharSet;
            }

            showInverse = (inverse && !isIIeRom) || (flash && m_flashOn);
            fbColOrigin = fbRowOrigin + col * charStride;

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
                    uint32_t color = (glyphRow & (1 << px)) ? m_onColor
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
