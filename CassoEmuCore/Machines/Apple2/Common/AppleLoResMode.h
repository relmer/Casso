#pragma once

#include "Pch.h"
#include "Video/VideoOutput.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLoResMode
//
//  40x48 16-color lo-res graphics renderer.
//
////////////////////////////////////////////////////////////////////////////////

class AppleLoResMode : public VideoOutput
{
public:
    explicit AppleLoResMode (MemoryBus & bus);

    void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) override;

    Word GetActivePageAddress (bool page2) const override;

    const char * GetModeName () const override { return "apple2-lores"; }

private:
    static Word GetRowBaseAddress (int row, Word pageBase);

    MemoryBus & m_bus;
};
