#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDoubleHiResMode
//
//  560x192 double hi-res from interleaved main/aux hi-res memory.
//  NOT YET IMPLEMENTED — renders monochrome hi-res fallback.
//  Full implementation requires aux memory interleaving via AppleIIeMmu.
//
////////////////////////////////////////////////////////////////////////////////

class AppleDoubleHiResMode : public VideoOutput
{
public:
    explicit AppleDoubleHiResMode (MemoryBus & bus);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-doublehires"; }

    // Aux memory (a pointer into the //e auxiliary 64K RAM block) is set by
    // the shell wiring. DHR reads aux + main interleaved per byte position
    // (aux supplies the first 7 dots, main supplies the next 7 dots).
    void SetAuxMemory (const Byte * auxMem) { m_auxMem = auxMem; }

private:
    MemoryBus    & m_bus;
    const Byte   * m_auxMem = nullptr;
};
