#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextMode
//
//  40x24 text mode renderer. Reads from text page memory ($0400 or $0800)
//  via MemoryBus, renders characters using embedded CharacterRom glyphs.
//
////////////////////////////////////////////////////////////////////////////////

class AppleTextMode : public VideoOutput
{
public:
    explicit AppleTextMode (MemoryBus & bus);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-text40"; }

    void SetFlashState (bool on) { m_flashOn = on; }

private:
    static Word RowBaseAddress (int row, Word pageBase);

    MemoryBus & m_bus;
    bool        m_flashOn    = true;
    uint32_t    m_frameCount = 0;
};
