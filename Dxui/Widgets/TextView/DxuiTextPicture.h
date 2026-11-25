#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPicture
//
//  A picture in a text view's rows, in 32-bit BGRA pixels, drawn a pixel a
//  DIP or smaller to fit the text's width. A picture with a ping has rings
//  spreading from that point every few seconds, and a zoomable one is
//  zoomed by the wheel and panned by a drag.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTextPicture
{
    std::vector<uint32_t>  bgra;
    int                    width  = 0;
    int                    height = 0;
    int                    pingX  = -1;      // in the picture's pixels; -1 for none
    int                    pingY  = -1;
    bool                   zoom   = false;
};
