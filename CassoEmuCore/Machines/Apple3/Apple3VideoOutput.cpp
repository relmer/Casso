#include "Pch.h"

#include "Apple3VideoOutput.h"
#include "Apple3Palette.h"
#include "Apple3TextCells.h"
#include "Apple3Video.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3VideoOutput
//
////////////////////////////////////////////////////////////////////////////////

Apple3VideoOutput::Apple3VideoOutput (const Apple3Video & video)
    : m_video (video)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetApple2Modes
//
////////////////////////////////////////////////////////////////////////////////

void Apple3VideoOutput::SetApple2Modes (AppleLoResMode * loRes, AppleHiResMode * hiRes)
{
    m_loRes = loRes;
    m_hiRes = hiRes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  The frame is 560 by 192; each of its lines fills fbHeight / 192 frame
//  buffer lines.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3VideoOutput::Render (const Byte * videoRam, uint32_t * framebuffer, int fbWidth, int fbHeight)
{
    UNREFERENCED_PARAMETER (videoRam);

    if (IsApple2Graphics())
    {
        RenderApple2Graphics (framebuffer, fbWidth, fbHeight);
    }
    else
    {
        m_video.Render (m_screen);
        DrawScreen (0, framebuffer, fbWidth, fbHeight);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsApple2Graphics
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3VideoOutput::IsApple2Graphics() const
{
    return m_video.IsEmulationMode() && !m_video.IsApple2Text() && m_video.IsScreenOn() &&
           m_loRes != nullptr && m_hiRes != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderApple2Graphics
//
//  The renderer reads the displayed page from the copy, which is laid out
//  as the Apple II's memory.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3VideoOutput::RenderApple2Graphics (uint32_t * framebuffer, int fbWidth, int fbHeight)
{
    bool           isHiRes = m_video.IsApple2HiRes();
    bool           isPage2 = m_video.IsPage2();
    VideoOutput  * mode    = isHiRes ? static_cast<VideoOutput *> (m_hiRes) : static_cast<VideoOutput *> (m_loRes);
    Word           first   = mode->GetActivePageAddress (isPage2);
    Word           end     = isHiRes ? (Word) (first + 0x2000) : (Word) (first + 0x0400);



    m_display.resize (kDisplayEnd);

    for (Word address = first; address < end; address++)
    {
        m_display[address] = m_video.PeekDisplay (address);
    }

    m_hiRes->SetMonochrome (m_isMonochrome);
    mode->SetPage2 (isPage2);
    mode->Render (m_display.data(), framebuffer, fbWidth, fbHeight);

    if (m_video.IsApple2Mixed())
    {
        m_video.Render (m_screen);
        DrawScreen (kMixedFirstLine, framebuffer, fbWidth, fbHeight);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawScreen
//
//  Draws the decoded frame's lines from firstLine down.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3VideoOutput::DrawScreen (int firstLine, uint32_t * framebuffer, int fbWidth, int fbHeight) const
{
    int  width  = std::min (fbWidth, Apple3Screen::kWidth);
    int  repeat = std::max (1, fbHeight / Apple3Screen::kHeight);



    for (int y = firstLine; y < Apple3Screen::kHeight; y++)
    {
        for (int copy = 0; copy < repeat && y * repeat + copy < fbHeight; copy++)
        {
            uint32_t  * row = framebuffer + (size_t) (y * repeat + copy) * (size_t) fbWidth;



            for (int x = 0; x < width; x++)
            {
                row[x] = Apple3Palette::GetPixel (m_screen.Get (x, y), m_isMonochrome);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivePageAddress
//
////////////////////////////////////////////////////////////////////////////////

Word Apple3VideoOutput::GetActivePageAddress (bool page2) const
{
    return page2 ? Apple3TextCells::kPage2 : Apple3TextCells::kPage1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeName
//
////////////////////////////////////////////////////////////////////////////////

const char * Apple3VideoOutput::GetModeName() const
{
    const IApple3VideoMode  * mode = m_video.GetMode();
    const char              * name = (mode != nullptr) ? mode->GetModeName() : "apple3-graphics";



    if (IsApple2Graphics())
    {
        name = m_video.IsApple2HiRes() ? m_hiRes->GetModeName() : m_loRes->GetModeName();
    }

    return name;
}
