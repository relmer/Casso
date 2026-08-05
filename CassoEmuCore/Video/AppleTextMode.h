#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"

class CharacterRomData;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextMode
//
//  40x24 text mode renderer. Reads from text page memory ($0400 or $0800)
//  via MemoryBus, renders characters using the supplied CharacterRomData.
//
////////////////////////////////////////////////////////////////////////////////

class AppleTextMode : public VideoOutput
{
public:
    explicit AppleTextMode (MemoryBus & bus);
    AppleTextMode (MemoryBus & bus, const CharacterRomData & charRom);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    // Render only rows [startRow, endRow). Used by the composed
    // mixed-mode path to redraw the bottom 4 text rows on top of a
    // graphics frame (FR-017a / FR-020). The full Render() path calls
    // the same helper, ensuring a single composed code path.
    void RenderRowRange (
        int          startRow,
        int          endRow,
        const Byte * videoRam,
        uint32_t   * framebuffer,
        int          fbWidth,
        int          fbHeight);

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-text40"; }

    void SetFlashState (bool on) { m_flashOn = on; }
    void SetAltCharSet (bool on) { m_altCharSet = on; }

    // Drop the dirty-row cache so the next Render() re-rasterizes every row.
    // The shell calls this when reuse would be unsafe: a video-mode transition
    // (the framebuffer last held graphics / another mode) or a monochrome
    // color mode (the shell's non-idempotent post-render tint would darken any
    // row we skipped). AppleTextMode ALSO self-invalidates when its own page /
    // charset / on-color / target-framebuffer changes.
    void InvalidateCache () { m_cacheValid = false; }

    // The lit-pixel color for text glyphs. Defaults to green for legacy
    // monochrome look (and existing tests); the shell overrides it to white
    // for a color monitor. In mono color modes the framebuffer is re-tinted
    // afterward, so the shell leaves this green there.
    void SetOnColor (uint32_t bgra) { m_onColor = bgra; }

private:
    static Word RowBaseAddress (int row, Word pageBase);

    // True if the 40 bytes hold any glyph that flashes with the flash clock
    // ($40-$7F, and only when ALTCHARSET is off -- //e alt charset suppresses
    // flash). Such rows must re-raster on a flash-phase flip even if unchanged.
    bool RowHasFlashChar (const Byte * rowBytes) const;

    static constexpr int     kGridCols = 40;
    static constexpr int     kGridRows = 24;

    MemoryBus              & m_bus;
    const CharacterRomData & m_charRom;
    bool                     m_flashOn    = true;
    bool                     m_altCharSet = false;
    uint32_t                 m_onColor    = 0xFF00FF00;   // BGRA green (default)

    // Dirty-row cache: the text bytes last rendered plus the state they were
    // rendered under. Render() re-rasterizes only the rows whose bytes changed
    // (or, on a flash-phase flip, the rows with a flashing glyph); the
    // framebuffer persists between frames, so untouched rows keep their pixels.
    Byte                     m_prevBytes[kGridCols * kGridRows] = {};
    const uint32_t         * m_prevFramebuffer                  = nullptr;
    bool                     m_cacheValid                       = false;
    bool                     m_prevPage2                        = false;
    bool                     m_prevAltChar                      = false;
    bool                     m_prevFlashOn                      = true;
    uint32_t                 m_prevOnColor                      = 0;
};
