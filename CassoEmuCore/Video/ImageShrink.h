#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageShrink
//
//  Makes a smaller copy of a picture with a box filter, for the history
//  strip's thumbnails and a machine state's thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

class ImageShrink
{
public:
    static void  Shrink (const DxuiIconImage & source, int width, int height, DxuiIconImage & outImage);
};
