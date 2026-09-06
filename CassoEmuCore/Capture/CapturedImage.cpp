#include "Pch.h"

#include "Capture/CapturedImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IsValid
//
//  Positive dimensions and enough bytes to cover them. The buffer is allowed
//  to be LARGER than the dimensions require -- a caller that reserved room and
//  filled part of it is fine -- but never smaller, which is the case that
//  reads off the end.
//
////////////////////////////////////////////////////////////////////////////////

bool CapturedImage::IsValid() const
{
    size_t   needed = 0;



    if (widthPx <= 0 || heightPx <= 0)
    {
        return false;
    }

    needed = (size_t) widthPx * heightPx * kBytesPerPixel;

    return bgra.size() >= needed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToRgbaImage
//
//  Swap the blue and red channels; green and alpha keep their positions.
//
//  Written as an explicit per-pixel swap rather than anything clever. This
//  runs once per screenshot, at human frequency, so there is nothing to win by
//  being smart and a channel order is the sort of thing that has to be
//  readable to stay right.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CapturedImage::ToRgbaImage (const CapturedImage & source, RgbaImage & outImage)
{
    HRESULT   hr    = S_OK;
    size_t    count = 0;
    size_t    i     = 0;
    bool      valid = false;



    valid = source.IsValid();
    CBRAEx (valid, E_INVALIDARG);

    count = (size_t) source.widthPx * source.heightPx;

    outImage.width  = source.widthPx;
    outImage.height = source.heightPx;
    outImage.rgba.resize (count * kBytesPerPixel);

    for (i = 0; i < count; i++)
    {
        outImage.rgba[i * 4 + 0] = source.bgra[i * 4 + 2];   // R
        outImage.rgba[i * 4 + 1] = source.bgra[i * 4 + 1];   // G
        outImage.rgba[i * 4 + 2] = source.bgra[i * 4 + 0];   // B
        outImage.rgba[i * 4 + 3] = source.bgra[i * 4 + 3];   // A
    }

Error:
    return hr;
}
