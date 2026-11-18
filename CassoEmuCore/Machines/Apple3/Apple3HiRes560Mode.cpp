#include "Pch.h"

#include "Apple3HiRes560Mode.h"
#include "Apple3GraphicsRows.h"
#include "Apple3Screen.h"





static constexpr int  s_kLineBytes = Apple3GraphicsRows::kLineBytes * 2;





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
////////////////////////////////////////////////////////////////////////////////

void Apple3HiRes560Mode::Decode (const Apple3VideoInput & input, Apple3Screen & screen) const
{
    Word  evenBuffer = input.isPage2 ? Apple3GraphicsRows::kBuffer3 : Apple3GraphicsRows::kBuffer1;
    Word  oddBuffer  = input.isPage2 ? Apple3GraphicsRows::kBuffer4 : Apple3GraphicsRows::kBuffer2;



    for (int line = 0; line < Apple3GraphicsRows::kLines; line++)
    {
        Word  evenLine = Apple3GraphicsRows::GetLineAddress (line, evenBuffer, input.scrollOffset);
        Word  oddLine  = Apple3GraphicsRows::GetLineAddress (line, oddBuffer,  input.scrollOffset);



        for (int column = 0; column < s_kLineBytes; column++)
        {
            Word  address = (Word) (((column % 2 == 0) ? evenLine : oddLine) + column / 2);
            Byte  dots    = Apple3GraphicsRows::Read (input, address);



            for (int dot = 0; dot < Apple3GraphicsRows::kByteDots; dot++)
            {
                screen.Set (column * Apple3GraphicsRows::kByteDots + dot, line, ((dots >> dot) & 1) ? Apple3Screen::kWhite : Apple3Screen::kBlack);
            }
        }
    }
}