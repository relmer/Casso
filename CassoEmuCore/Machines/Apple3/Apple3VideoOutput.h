#pragma once

#include "Pch.h"

#include "Apple3Screen.h"
#include "Video/VideoOutput.h"

class AppleHiResMode;
class AppleLoResMode;
class Apple3Video;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3VideoOutput
//
//  The Apple ///'s display as one of the emulator's video outputs: the
//  frame Apple3Video decodes, drawn through the palette, each line twice
//  when the frame buffer is twice the display's height.
//
//  Under the Apple II lockout, lo-res and hi-res are drawn by the Apple II
//  renderers from a copy of the display memory, and the text of a mixed
//  screen by Apple3Video over the bottom four rows.
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
    void          SetApple2Modes       (AppleLoResMode * loRes, AppleHiResMode * hiRes);

private:
    static constexpr int   kMixedFirstLine = 160;
    static constexpr Word  kDisplayEnd     = 0x6000;

    bool          IsApple2Graphics     () const;
    void          RenderApple2Graphics (uint32_t * framebuffer, int fbWidth, int fbHeight);
    void          DrawScreen           (int firstLine, uint32_t * framebuffer, int fbWidth, int fbHeight) const;

    const Apple3Video  & m_video;
    Apple3Screen         m_screen;
    AppleLoResMode     * m_loRes        = nullptr;
    AppleHiResMode     * m_hiRes        = nullptr;
    std::vector<Byte>    m_display;
    bool                 m_isMonochrome = false;
};
