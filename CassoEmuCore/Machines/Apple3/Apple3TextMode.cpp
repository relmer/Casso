#include "Pch.h"

#include "Apple3TextMode.h"
#include "Apple3Screen.h"
#include "Apple3TextCells.h"





static constexpr int  s_kDotWidth = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
////////////////////////////////////////////////////////////////////////////////

void Apple3TextMode::Decode (const Apple3VideoInput & input, Apple3Screen & screen) const
{
    Word                   page = input.isPage2 ? Apple3TextCells::kPage2 : Apple3TextCells::kPage1;
    Apple3TextCells::Cell  cell;



    cell.foreground = Apple3Screen::kWhite;
    cell.background = Apple3Screen::kBlack;
    cell.canFlash   = true;
    cell.dotWidth   = s_kDotWidth;

    for (int row = 0; row < Apple3TextCells::kRows; row++)
    {
        Word  rowAddress = Apple3TextCells::GetRowAddress (row, page);



        for (int column = 0; column < Apple3TextCells::kColumns; column++)
        {
            cell.character = input.readSystem ((Word) (rowAddress + column));
            cell.x         = column * Apple3TextCells::kCellDots * s_kDotWidth;
            cell.row       = row;

            Apple3TextCells::DrawCell (input, cell, screen);
        }
    }
}
