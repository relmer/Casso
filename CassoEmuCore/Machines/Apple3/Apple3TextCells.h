#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"

class Apple3Screen;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3TextCells
//
//  The text modes' shared layout and cell drawing. The 24 rows are laid out
//  in a page as on the Apple II. A character with bit 7 clear is inverse;
//  bit 7 of each of its glyph rows selects steady inverse (0) or flashing
//  (1). Flashing is in the 40-column black and white mode only; the other
//  modes draw a flashing row as inverse.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3TextCells
{
public:
    static constexpr int   kRows        = 24;
    static constexpr int   kColumns     = 40;
    static constexpr int   kCellLines   = 8;
    static constexpr int   kCellDots    = 7;
    static constexpr Word  kPage1       = 0x0400;
    static constexpr Word  kPage2       = 0x0800;

    struct Cell
    {
        Byte  character  = 0;
        Byte  foreground = 0;
        Byte  background = 0;
        bool  canFlash   = false;
        int   x          = 0;
        int   row        = 0;
        int   dotWidth   = 1;
    };

    static Word  GetRowAddress (int row, Word page);
    static void  DrawCell      (const Apple3VideoInput & input, const Cell & cell, Apple3Screen & screen);
};
