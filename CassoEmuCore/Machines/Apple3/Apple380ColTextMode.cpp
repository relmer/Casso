#include "Pch.h"

#include "Apple380ColTextMode.h"
#include "Apple3Screen.h"
#include "Apple3TextCells.h"





static constexpr int  s_kColumns = Apple3TextCells::kColumns * 2;





////////////////////////////////////////////////////////////////////////////////
//
//  Decode
//
////////////////////////////////////////////////////////////////////////////////

void Apple380ColTextMode::Decode (const Apple3VideoInput & input, Apple3Screen & screen) const
{
    Word                   evenPage = input.isPage2 ? Apple3TextCells::kPage2 : Apple3TextCells::kPage1;
    Word                   oddPage  = input.isPage2 ? Apple3TextCells::kPage1 : Apple3TextCells::kPage2;
    Apple3TextCells::Cell  cell;



    cell.foreground = Apple3Screen::kWhite;
    cell.background = Apple3Screen::kBlack;

    for (int row = 0; row < Apple3TextCells::kRows; row++)
    {
        Word  evenRow = Apple3TextCells::GetRowAddress (row, evenPage);
        Word  oddRow  = Apple3TextCells::GetRowAddress (row, oddPage);



        for (int column = 0; column < s_kColumns; column++)
        {
            Word  rowAddress = (column % 2 == 0) ? evenRow : oddRow;



            cell.character = input.readSystem ((Word) (rowAddress + column / 2));
            cell.x         = column * Apple3TextCells::kCellDots;
            cell.row       = row;

            Apple3TextCells::DrawCell (input, cell, screen);
        }
    }
}
