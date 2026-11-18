#include "Pch.h"

#include "Apple3TextCells.h"
#include "Apple3Screen.h"
#include "IGlyphSource.h"





static constexpr int   s_kRowsPerThird  = 8;
static constexpr Word  s_kLineStride    = 0x80;
static constexpr Word  s_kThirdStride   = 0x28;
static constexpr Byte  s_kNormalBit     = 0x80;
static constexpr Byte  s_kFlashBit      = 0x80;
static constexpr Byte  s_kCodeMask      = 0x7F;





////////////////////////////////////////////////////////////////////////////////
//
//  GetRowAddress
//
//  A text row's first byte: the rows of each third of the screen are $80
//  apart, and the thirds are $28 apart.
//
////////////////////////////////////////////////////////////////////////////////

Word Apple3TextCells::GetRowAddress (int row, Word page)
{
    return (Word) (page + (row % s_kRowsPerThird) * s_kLineStride + (row / s_kRowsPerThird) * s_kThirdStride);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawCell
//
//  Draws one character's eight lines of seven dots, each dot dotWidth pixels
//  wide. A lit dot is the foreground and an unlit one the background;
//  inverse swaps them.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3TextCells::DrawCell (const Apple3VideoInput & input, const Cell & cell, Apple3Screen & screen)
{
    Byte  code      = (Byte) (cell.character & s_kCodeMask);
    bool  isInverse = (cell.character & s_kNormalBit) == 0;



    for (int line = 0; line < kCellLines; line++)
    {
        Byte  glyph      = input.glyphs.GetGlyphRow (code, (line + input.scrollOffset) % kCellLines);
        bool  isFlashing = cell.canFlash && (glyph & s_kFlashBit) != 0;
        bool  isInverted = isInverse && (!isFlashing || input.isFlashOn);
        int   y          = cell.row * kCellLines + line;



        for (int dot = 0; dot < kCellDots; dot++)
        {
            bool  isLit = (((glyph >> dot) & 1) != 0) != isInverted;
            Byte  color = isLit ? cell.foreground : cell.background;



            for (int i = 0; i < cell.dotWidth; i++)
            {
                screen.Set (cell.x + dot * cell.dotWidth + i, y, color);
            }
        }
    }
}
