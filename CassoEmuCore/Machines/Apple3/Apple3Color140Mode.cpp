#include "Pch.h"

#include "Apple3Color140Mode.h"
#include "Apple3GraphicsRows.h"
#include "Apple3Screen.h"





static constexpr int       s_kGroupBytes   = 4;
static constexpr int       s_kGroupPixels  = 7;
static constexpr int       s_kPixelBits    = 4;
static constexpr int       s_kPixelWidth   = 4;
static constexpr int       s_kGroups       = Apple3GraphicsRows::kLineBytes * 2 / s_kGroupBytes;
static constexpr uint32_t  s_kDotsMask     = 0x7F;
static constexpr uint32_t  s_kPixelMask    = 0x0F;





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Color140Mode::Decode (const Apple3VideoInput & input, Apple3Screen & screen) const
{
    Word  evenBuffer = input.isPage2 ? Apple3GraphicsRows::kBuffer3 : Apple3GraphicsRows::kBuffer1;
    Word  oddBuffer  = input.isPage2 ? Apple3GraphicsRows::kBuffer4 : Apple3GraphicsRows::kBuffer2;



    for (int line = 0; line < Apple3GraphicsRows::kLines; line++)
    {
        Word  evenLine = Apple3GraphicsRows::GetLineAddress (line, evenBuffer, input.scrollOffset);
        Word  oddLine  = Apple3GraphicsRows::GetLineAddress (line, oddBuffer,  input.scrollOffset);



        for (int group = 0; group < s_kGroups; group++)
        {
            uint32_t  bits = 0;



            for (int i = 0; i < s_kGroupBytes; i++)
            {
                int   column  = group * s_kGroupBytes + i;
                Word  address = (Word) (((column % 2 == 0) ? evenLine : oddLine) + column / 2);



                bits |= (Apple3GraphicsRows::Read (input, address) & s_kDotsMask) << (i * Apple3GraphicsRows::kByteDots);
            }

            for (int pixel = 0; pixel < s_kGroupPixels; pixel++)
            {
                Byte  color = (Byte) ((bits >> (pixel * s_kPixelBits)) & s_kPixelMask);
                int   x     = (group * s_kGroupPixels + pixel) * s_kPixelWidth;



                for (int i = 0; i < s_kPixelWidth; i++)
                {
                    screen.Set (x + i, line, color);
                }
            }
        }
    }
}