/*
 *  bar.h -- the bar across the text screen, drawn by bar.c
 */

#ifndef BAR_H
#define BAR_H

#define BAR_LENGTH  40      /* columns in the bar, and the count's limit */

void  draw_bar (unsigned char row, unsigned char length, char fill);

#endif
