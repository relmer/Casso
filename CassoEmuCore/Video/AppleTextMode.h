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

private:
    static Word RowBaseAddress (int row, Word pageBase);

    MemoryBus              & m_bus;
    const CharacterRomData & m_charRom;
    bool                     m_flashOn    = true;
    bool                     m_altCharSet = false;
    uint32_t                 m_frameCount = 0;
};
