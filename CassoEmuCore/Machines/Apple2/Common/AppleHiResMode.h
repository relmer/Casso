#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleHiResMode
//
//  280x192 hi-res graphics with NTSC color artifact simulation.
//
////////////////////////////////////////////////////////////////////////////////

class AppleHiResMode : public VideoOutput
{
public:
    explicit AppleHiResMode (MemoryBus & bus);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    Word GetActivePageAddress (bool page2) const override;

    // Monochrome decode, pushed in by the shell from the selected monitor.
    // There is no soft switch for it -- on real hardware the monitor is the
    // thing that does or does not produce color.
    //
    // This is a different decode, not a filter over the color one. Artifact
    // color IS the information a color monitor makes from the dot stream;
    // luminance-tinting it renders an isolated dot at ~57% brightness where
    // hardware shows full white, and hides the half-dot shift entirely.
    void SetMonochrome (bool mono) { m_monochrome = mono; }
    bool IsMonochrome  () const    { return m_monochrome; }

    const char * GetModeName () const override { return "apple2-hires"; }

    static Word GetScanlineAddress (int scanline, Word pageBase);

    // Main RAM, read directly in preference to the bus; see ReadScreenByte.
    void SetMainMemory (const Byte * mainMem) { m_mainMem = mainMem; }

private:
    Byte ReadScreenByte (const Byte * videoRam, Word addr) const;

    MemoryBus  & m_bus;
    const Byte * m_mainMem    = nullptr;
    bool         m_monochrome = false;
};
