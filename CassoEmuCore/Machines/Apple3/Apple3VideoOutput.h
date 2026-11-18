#pragma once

#include "Pch.h"

#include "Apple3Screen.h"
#include "Video/VideoOutput.h"

class Apple3Video;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3VideoOutput
//
//  The Apple ///'s display as one of the emulator's video outputs: the
//  frame Apple3Video decodes, drawn through the palette, each line twice
//  when the frame buffer is twice the display's height.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3VideoOutput : public VideoOutput
{
public:
    explicit Apple3VideoOutput (const Apple3Video & video);

    void          Render               (const Byte * videoRam, uint32_t * framebuffer, int fbWidth, int fbHeight) override;
    Word          GetActivePageAddress (bool page2) const override;
    const char  * GetModeName          () const override;

    void          SetMonochrome        (bool isMonochrome) { m_isMonochrome = isMonochrome; }

private:
    const Apple3Video  & m_video;
    Apple3Screen         m_screen;
    bool                 m_isMonochrome = false;
};
