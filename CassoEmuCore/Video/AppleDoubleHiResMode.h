#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDoubleHiResMode
//
//  560x192 double hi-res from interleaved main/aux hi-res memory.
//
//  Two decodes, picked by the monitor the user selected. A color monitor
//  gets the 16-color decode, where four consecutive dots form the nibble
//  that indexes the DHR palette. A monochrome monitor gets all 560 dots at
//  full horizontal resolution, one dot per framebuffer pixel, because that
//  is what the hardware puts on a monochrome monitor: the color is an NTSC
//  chroma artifact that a monochrome CRT never produces. Software written
//  for 560x192 mono -- which is most DHR art that uses dithering for
//  shading -- is unreadable through the color decode, since each 4-dot cell
//  collapses to a single color and the dither pattern disappears.
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

    // Monochrome decode, pushed in by the shell from the selected monitor
    // rather than read from a soft switch -- no soft switch exists, because
    // on real hardware this is a property of the display, not the machine.
    // Lit dots come out white so the shell's phosphor tint can recolor them
    // at full brightness.
    void SetMonochrome (bool mono) { m_monochrome = mono; }
    bool IsMonochrome  () const    { return m_monochrome; }

private:
    MemoryBus    & m_bus;
    const Byte   * m_auxMem     = nullptr;
    bool           m_monochrome = false;
};
