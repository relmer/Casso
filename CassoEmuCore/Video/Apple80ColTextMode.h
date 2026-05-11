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
    void SetAuxMemory  (const Byte * auxMem) { m_auxMem      = auxMem; }
    void SetFlashState (bool on)             { m_flashOn     = on; }
    void SetAltCharSet (bool on)             { m_altCharSet  = on; }

private:
    MemoryBus              & m_bus;
    const CharacterRomData & m_charRom;
    const Byte             * m_auxMem      = nullptr;
    bool                     m_flashOn     = true;
    bool                     m_altCharSet  = false;
    uint32_t                 m_frameCount  = 0;
};
