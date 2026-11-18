#pragma once

#include "Pch.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "DxuiTextRow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRowLayout
//
//  The parts of DxuiTextView's layout that depend on a row alone: how wide
//  a picture row is drawn and how many lines it takes, the indent of a
//  row's wrapped lines, and the font weight of a run's styles.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextRowLayout
{
public:
    //  Pixels on screen for each of the picture's: a DIP's, `pxPerDip`, or
    //  fewer to fit `widthPx`.
    static float  GetPictureScale (const DxuiTextPicture & picture, float widthPx, float pxPerDip);

    //  The lines of `lineHeightPx` the picture takes, at least one.
    static int    GetPictureLines (const DxuiTextPicture & picture, float widthPx, float pxPerDip, int lineHeightPx);

    //  The picture with its top left at `left` and `top`.
    static void   PaintPicture    (IDxuiTextRenderer & text, const DxuiTextPicture & picture, float left, float top, float widthPx, float pxPerDip);

    //  A row's wrap indent in a last cell `avail` characters wide, or zero
    //  when less than half of that width would remain for the wrapped text.
    static int    GetWrapIndent   (const DxuiTextRow & row, int avail);

    //  The weight of a run bold, italic, both or neither.
    static DxuiFontWeight  GetWeight (bool bold, bool italic);
};
