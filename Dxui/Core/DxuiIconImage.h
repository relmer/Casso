#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiIconImage
//
//  A small picture drawn beside a row's text: premultiplied BGRA, top row
//  first, width by height.
//
//  PIXELS, NOT A HANDLE. Where the picture came from -- the shell, a resource,
//  a test -- is the host's business, so a widget that shows one stays free of
//  HICONs and can be exercised with no desktop at all. Shared and immutable,
//  since a folder's worth of rows usually shows a handful of distinct icons.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiIconImage
{
    int                    width  = 0;
    int                    height = 0;
    std::vector<uint32_t>  bgraPremul;

    //  Rasterizes an icon at sizePx square. The handle stays the caller's.
    static HRESULT  FromHicon (HICON icon, int sizePx, DxuiIconImage & outImage);

private:
    static HRESULT  ApplyMask (HDC dc, HICON icon, int sizePx, uint32_t * bits, DxuiIconImage & image);
};
