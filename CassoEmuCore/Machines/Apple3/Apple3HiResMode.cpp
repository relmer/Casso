#include "Pch.h"

#include "Apple3HiResMode.h"
#include "Apple3GraphicsRows.h"
#include "Apple3Screen.h"





static constexpr int  s_kDotWidth = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
////////////////////////////////////////////////////////////////////////////////

void Apple3HiResMode::Decode (const Apple3VideoInput & input, Apple3Screen & screen) const
{
    Word  buffer = input.isPage2 ? Apple3GraphicsRows::kBuffer2 : Apple3GraphicsRows::kBuffer1;



    for (int line = 0; line < Apple3GraphicsRows::kLines; line++)
    {
        Word  address = Apple3GraphicsRows::GetLineAddress (line, buffer, input.scrollOffset);



        for (int column = 0; column < Apple3GraphicsRows::kLineBytes; column++)
        {
            Byte  dots = Apple3GraphicsRows::Read (input, (Word) (address + column));



            for (int dot = 0; dot < Apple3GraphicsRows::kByteDots; dot++)
            {
                Byte  color = ((dots >> dot) & 1) ? Apple3Screen::kWhite : Apple3Screen::kBlack;
                int   x     = (column * Apple3GraphicsRows::kByteDots + dot) * s_kDotWidth;



                screen.Set (x,     line, color);
                screen.Set (x + 1, line, color);
            }
        }
    }
}