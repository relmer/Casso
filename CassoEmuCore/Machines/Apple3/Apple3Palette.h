#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Palette
//
//  The Apple ///'s 16 colors, the Apple II lo-res set in the same order, and
//  the 16 gray levels of the black and white output. The monochrome output
//  sums the four color bits through binary-weighted resistors, so the gray
//  level is the color number: 0 is black and 15 white.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Palette
{
public:
    static constexpr int  kColorCount = 16;

    //  A color number as a BGRA pixel: its color, or its gray level for a
    //  monochrome monitor.
    static uint32_t  GetPixel (Byte color, bool isMonochrome);
};
