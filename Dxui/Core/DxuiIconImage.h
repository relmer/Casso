#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiIconImage
//
//  A small image drawn beside a row's text: premultiplied BGRA, top row
//  first, width by height.
//
//  PIXELS, NOT A HANDLE. The source of the image (the shell, a resource, a
//  test) is up to the host, so widgets that draw one use no HICONs and can be
//  tested without a desktop. Shared and immutable, since the rows of a folder
//  typically use only a few distinct icons.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiIconImage
{
    int                    width  = 0;
    int                    height = 0;
    std::vector<uint32_t>  bgraPremul;

    //  Rasterizes an icon at sizePx square. The handle stays the caller's.
    static HRESULT  FromHicon (HICON icon, int sizePx, DxuiIconImage & outImage);

    //  `source` scaled to sizePx square with a bilinear filter, each pixel
    //  from the four nearest to its center, as Explorer scales a larger
    //  icon's art down to a row's size.
    static DxuiIconImage  Resample (const DxuiIconImage & source, int sizePx);

private:
    static HRESULT  ApplyMask (HDC dc, HICON icon, int sizePx, uint32_t * bits, DxuiIconImage & image);
};
