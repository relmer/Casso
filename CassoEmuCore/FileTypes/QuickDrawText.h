#pragma once

#include "Pch.h"

#include "FileTypes/QuickDrawCanvas.h"


class QuickDrawFont;





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawTextStyle
//
//  A port's text settings. The face bits are bold, italic, underline,
//  outline and shadow. The mode is a transfer mode: 0 to 3 copy, OR, XOR
//  and BIC the character's whole box, its set pixels in the foreground
//  color and the rest in the background color; 4 to 7 do the same with the
//  set pixels alone; $8000 inverts the colors first. The extras are pixels
//  added to a space's advance and to every character's. With font flag
//  bit 0 set, every character has the font's widest advance.
//
////////////////////////////////////////////////////////////////////////////////

struct QuickDrawTextStyle
{
    Word    family    = 0xFFFE;     // Shaston
    int     size      = 8;
    Word    face      = 0;
    int     mode      = 0;
    int     foreColor = 0;
    int     backColor = 0x0F;
    double  spExtra   = 0.0;
    double  chExtra   = 0.0;
    Word    fontFlags = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawText
//
//  A run of text drawn onto a canvas in a bitmap font, its baseline's left
//  end at a point of the port.
//
////////////////////////////////////////////////////////////////////////////////

class QuickDrawText
{
public:
    static constexpr Word  kBold      = 0x01;
    static constexpr Word  kItalic    = 0x02;
    static constexpr Word  kUnderline = 0x04;
    static constexpr Word  kOutline   = 0x08;
    static constexpr Word  kShadow    = 0x10;

    static void  Draw (QuickDrawCanvas & canvas, const QuickDrawFont & font, const QuickDrawTextStyle & style, QuickDrawCanvas::Point at, std::span<const Byte> text);

    static const wchar_t *  GetFamilyName (Word family);

private:
    //  A character's pixels with the face applied: set is the foreground,
    //  and the box from left to right, top to bottom, is what a copy mode
    //  covers. Coordinates are from the pen, y from the baseline.
    struct Cell
    {
        int                            left    = 0;
        int                            right   = 0;
        int                            top     = 0;
        int                            bottom  = 0;
        int                            advance = 0;
        std::set<std::pair<int, int>>  set;
    };

    static void  BuildCell (const QuickDrawFont & font, const QuickDrawTextStyle & style, Byte character, Cell & outCell);
    static void  DrawCell  (QuickDrawCanvas & canvas, const QuickDrawTextStyle & style, const Cell & cell, int penH, int baseV, bool set);
};
