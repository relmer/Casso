#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VideoOutput
//
//  Abstract interface for video mode renderers.
//
////////////////////////////////////////////////////////////////////////////////

class VideoOutput
{
public:
    virtual ~VideoOutput () = default;

    virtual void Render (
        const Byte * videoRam,
        uint32_t * framebuffer,
        int fbWidth,
        int fbHeight) = 0;

    virtual Word GetActivePageAddress (bool page2) const = 0;

    virtual const char * GetModeName () const = 0;
};
