#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"

class CharacterRomData;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple80ColTextMode
//
//  80x24 text renderer using interleaved main/aux memory.
//
////////////////////////////////////////////////////////////////////////////////

class Apple80ColTextMode : public VideoOutput
{
public:
    explicit Apple80ColTextMode (MemoryBus & bus);
    Apple80ColTextMode (MemoryBus & bus, const CharacterRomData & charRom);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    // Render only rows [startRow, endRow). Used by the composed
    // mixed-mode path (FR-017a / FR-020) to draw the bottom 4 text
    // rows on top of a graphics frame when 80COL is active. The full
    // Render() path calls this same helper to ensure a single composed
    // code path (no branched duplication).
    void RenderRowRange (
        int          startRow,
        int          endRow,
        const Byte * videoRam,
        uint32_t   * framebuffer,
        int          fbWidth,
        int          fbHeight);

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-text80"; }

    // Provide access to aux memory for 80-column interleaved rendering.
    // videoRam (passed to Render) is main RAM; aux is set separately.
    //
    // Main is taken directly as well, for the same reason double hi-res
    // takes it: the bus follows live banking, and under 80STORE with PAGE2
    // on it answers a text-page read from aux. The mixed-mode overlay passes
    // no videoRam, so without this pointer a frame scanned while a program
    // had PAGE2 on for its own aux writes showed aux in both columns.
    void SetAuxMemory  (const Byte * auxMem)  { m_auxMem      = auxMem; }
    void SetMainMemory (const Byte * mainMem) { m_mainMem     = mainMem; }
    void SetFlashState (bool on)             { m_flashOn     = on; }
    void SetAltCharSet (bool on)             { m_altCharSet  = on; }

    // Lit-pixel color for text glyphs (green default; shell sets white for a
    // color monitor). Mono color modes re-tint afterward, so green stays there.
    void SetOnColor    (uint32_t bgra)       { m_onColor     = bgra; }

    // Drop the dirty-row cache so the next Render() re-rasterizes every row.
    // See AppleTextMode::InvalidateCache -- the shell calls this on a video-
    // mode transition or in a monochrome color mode; the mode also self-
    // invalidates when its aux pointer / charset / on-color changes.
    void InvalidateCache () { m_cacheValid = false; }

private:
    // True if the 40+40 interleaved bytes hold a glyph that flashes with the
    // flash clock ($40-$7F, only when ALTCHARSET is off). Such rows re-raster
    // on a flash-phase flip even if no byte changed.
    bool HasFlashChar (const Byte * rowBytes) const;

    // The character at addr in one bank: aux for even columns, main for odd,
    // read from the bank's own buffer when known, and only otherwise through
    // the bus (see SetMainMemory).
    Byte ReadTextByte (bool fromAux, const Byte * videoRam, Word addr) const;

    static constexpr int     kGridCols = 80;
    static constexpr int     kGridRows = 24;

    MemoryBus              & m_bus;
    const CharacterRomData & m_charRom;
    const Byte             * m_auxMem      = nullptr;
    const Byte             * m_mainMem     = nullptr;
    bool                     m_flashOn     = true;
    bool                     m_altCharSet  = false;
    uint32_t                 m_onColor     = 0xFF00FF00;   // BGRA green (default)

    // Dirty-row cache (see AppleTextMode for the model). 80-col reads even
    // columns from aux and odd from main, so a row's signature is its 80
    // effective char codes; the aux pointer is part of the cached state.
    Byte                     m_prevBytes[kGridCols * kGridRows] = {};
    const uint32_t         * m_prevFramebuffer                  = nullptr;
    const Byte             * m_prevAuxMem                       = nullptr;
    bool                     m_cacheValid                       = false;
    bool                     m_prevAltChar                      = false;
    bool                     m_prevFlashOn                      = true;
    uint32_t                 m_prevOnColor                      = 0;
};
