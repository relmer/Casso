/*
 *  bar.c -- draws the bar for cdemo.c
 *
 *  A separate file, so stepping into draw_bar from main moves the source
 *  pane to another file.
 */

#include <conio.h>

#include "bar.h"



unsigned char  barDraws;    /* calls to draw_bar, wrapping at 256 */



/*
 *  draw_bar
 *
 *  Draws `length` copies of `fill` from column 0 of `row`, then spaces to
 *  the right edge, so a shorter bar erases a longer one.
 */

void draw_bar (unsigned char row, unsigned char length, char fill)
{
    unsigned char  column;
    char           cell;


    for (column = 0; column < BAR_LENGTH; ++column)
    {
        if (column < length)
        {
            cell = fill;
        }
        else
        {
            cell = ' ';
        }

        cputcxy (column, row, cell);
    }

    ++barDraws;
}
