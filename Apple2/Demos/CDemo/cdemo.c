/*
 *  cdemo.c -- a small C program for trying the debugger's source-level
 *  debugging of a cc65 program
 *
 *  Direct-boot demo: the build (Build.ps1 in this folder) compiles this file
 *  and bar.c with cc65's -g, links them at $6000 behind boot.s, and writes
 *  cdemo.dbg beside the sources. The .dbg has the paths of the sources
 *  relative to itself, so they stay in one folder.
 *
 *  What it does: draws a title, then counts from 0 to 39 forever. Each pass
 *  draws the count, grows the bar across the screen by one cell, and waits.
 *  Each time the bar fills, `passes` goes up and the next hint from the
 *  string table appears.
 *
 *  To step through, break on and watch:
 *      globals      count, passes, hintIndex, and barDraws in bar.c
 *      locals       draw_number's digits and length, draw_bar's column
 *      arguments    draw_bar's row, length and fill
 *      a table      hints[], an array of string pointers
 */

#include <conio.h>

#include "bar.h"



#define TITLE_ROW    0
#define COUNT_ROW   10
#define BAR_ROW     12
#define PASSES_ROW  14
#define HINT_ROW    18
#define WAIT_TICKS  300     /* delay loop count between steps */



unsigned char  count;       /* loop counter, 0 to BAR_LENGTH - 1 */
unsigned int   passes;      /* times the bar has filled */
unsigned char  hintIndex;   /* the entry of hints[] on screen */

static const char * const hints[] =
{
    "Set a breakpoint on a line of cdemo.c",
    "Step into draw_bar to open bar.c",
    "Watch count and passes change",
    "Hover a variable for its value",
};

#define HINT_COUNT  (sizeof (hints) / sizeof (hints[0]))



/*
 *  draw_number
 *
 *  Draws `value` in decimal at column `x` of row `y`, followed by two
 *  spaces to erase a longer number drawn before it.
 */

static void draw_number (unsigned char x, unsigned char y, unsigned int value)
{
    char           digits[5];
    unsigned char  length;


    length = 0;

    do
    {
        digits[length] = (char) ('0' + value % 10);
        ++length;
        value /= 10;
    }
    while (value != 0);

    gotoxy (x, y);

    while (length > 0)
    {
        --length;
        cputc (digits[length]);
    }

    cputs ("  ");
}



/*
 *  draw_hint
 *
 *  Clears the hint row and draws hints[hintIndex] on it.
 */

static void draw_hint (void)
{
    cclearxy (0, HINT_ROW, BAR_LENGTH);
    cputsxy  (0, HINT_ROW, hints[hintIndex]);
}



/*
 *  wait
 *
 *  Spins `ticks` times, so each step is slow enough to watch.
 */

static void wait (unsigned int ticks)
{
    unsigned int  tick;


    for (tick = 0; tick < ticks; ++tick)
    {
    }
}



int main (void)
{
    clrscr ();
    cputsxy (0, TITLE_ROW,     "Casso C demo: cc65 source-level debug");
    cputsxy (0, COUNT_ROW,     "Count:");
    cputsxy (0, PASSES_ROW,    "Passes:");
    cputsxy (0, PASSES_ROW + 2, "Hint:");
    draw_hint ();

    for (;;)
    {
        draw_number (7, COUNT_ROW,  count);
        draw_number (8, PASSES_ROW, passes);
        draw_bar    (BAR_ROW, (unsigned char) (count + 1), '*');
        wait        (WAIT_TICKS);

        ++count;

        if (count == BAR_LENGTH)
        {
            count = 0;
            ++passes;

            ++hintIndex;

            if (hintIndex == HINT_COUNT)
            {
                hintIndex = 0;
            }

            draw_hint ();
        }
    }
}
