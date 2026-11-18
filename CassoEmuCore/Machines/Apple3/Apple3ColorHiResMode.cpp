#include "Pch.h"

#include "Apple3ColorHiResMode.h"
#include "Apple3GraphicsRows.h"
#include "Apple3Screen.h"





static constexpr int   s_kDotWidth        = 2;
static constexpr int   s_kForegroundShift = 4;
static constexpr Byte  s_kColorMask       = 0x0F;





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
////////////////////////////////////////////////////////////////////////////////

void Apple3ColorHiResMode::Decode (const Apple3VideoInput & input, Apple3Screen & screen) const
{
    Word  dotBuffer   = input.isPage2 ? Apple3GraphicsRows::kBuffer2 : Apple3GraphicsRows::kBuffer1;
    Word  colorBuffer = input.isPage2 ? Apple3GraphicsRows::kBuffer1 : Apple3GraphicsRows::kBuffer2;



    for (int line = 0; line < Apple3GraphicsRows::kLines; line++)
    {
        Word  dotLine   = Apple3GraphicsRows::GetLineAddress (line, dotBuffer,   input.scrollOffset);
        Word  colorLine = Apple3GraphicsRows::GetLineAddress (line, colorBuffer, input.scrollOffset);



        for (int column = 0; column < Apple3GraphicsRows::kLineBytes; column++)
        {
            Byte  dots       = Apple3GraphicsRows::Read (input, (Word) (dotLine   + column));
            Byte  colors     = Apple3GraphicsRows::Read (input, (Word) (colorLine + column));
            Byte  foreground = (Byte) (colors >> s_kForegroundShift);
            Byte  background = (Byte) (colors & s_kColorMask);



            for (int dot = 0; dot < Apple3GraphicsRows::kByteDots; dot++)
            {
                Byte  color = ((dots >> dot) & 1) ? foreground : background;
                int   x     = (column * Apple3GraphicsRows::kByteDots + dot) * s_kDotWidth;



                screen.Set (x,     line, color);
                screen.Set (x + 1, line, color);
            }
        }
    }
}