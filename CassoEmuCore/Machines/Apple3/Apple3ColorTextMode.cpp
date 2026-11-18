#include "Pch.h"

#include "Apple3ColorTextMode.h"
#include "Apple3Screen.h"
#include "Apple3TextCells.h"





static constexpr int   s_kDotWidth        = 2;
static constexpr int   s_kForegroundShift = 4;
static constexpr Byte  s_kColorMask       = 0x0F;





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
////////////////////////////////////////////////////////////////////////////////

void Apple3ColorTextMode::Decode (const Apple3VideoInput & input, Apple3Screen & screen) const
{
    Word                   characterPage = input.isPage2 ? Apple3TextCells::kPage2 : Apple3TextCells::kPage1;
    Word                   colorPage     = input.isPage2 ? Apple3TextCells::kPage1 : Apple3TextCells::kPage2;
    Apple3TextCells::Cell  cell;



    cell.dotWidth = s_kDotWidth;

    for (int row = 0; row < Apple3TextCells::kRows; row++)
    {
        Word  characterRow = Apple3TextCells::GetRowAddress (row, characterPage);
        Word  colorRow     = Apple3TextCells::GetRowAddress (row, colorPage);



        for (int column = 0; column < Apple3TextCells::kColumns; column++)
        {
            Byte  color = input.readSystem ((Word) (colorRow + column));



            cell.character  = input.readSystem ((Word) (characterRow + column));
            cell.foreground = (Byte) (color >> s_kForegroundShift);
            cell.background = (Byte) (color & s_kColorMask);
            cell.x          = column * Apple3TextCells::kCellDots * s_kDotWidth;
            cell.row        = row;

            Apple3TextCells::DrawCell (input, cell, screen);
        }
    }
}
