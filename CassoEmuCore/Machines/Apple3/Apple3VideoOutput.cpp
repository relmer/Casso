#include "Pch.h"

#include "Apple3VideoOutput.h"
#include "Apple3Palette.h"
#include "Apple3TextCells.h"
#include "Apple3Video.h"





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
//  Render
//
//  The frame is 560 by 192; each of its lines fills fbHeight / 192 frame
//  buffer lines.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3VideoOutput::Render (const Byte * videoRam, uint32_t * framebuffer, int fbWidth, int fbHeight)
{
    int  width       = std::min (fbWidth, Apple3Screen::kWidth);
    int  repeat      = std::max (1, fbHeight / Apple3Screen::kHeight);



    UNREFERENCED_PARAMETER (videoRam);

    m_video.Render (m_screen);

    for (int y = 0; y < Apple3Screen::kHeight; y++)
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



    return (mode != nullptr) ? mode->GetModeName() : "apple3-graphics";
}
