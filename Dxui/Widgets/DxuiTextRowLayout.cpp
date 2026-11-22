#include "Pch.h"

#include "DxuiTextRowLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRowLayout::GetPictureScale
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTextRowLayout::GetPictureScale (const DxuiTextView::Picture & picture, float widthPx, float pxPerDip)
{
    float  scale = pxPerDip;



    if (picture.width > 0 && (float) picture.width * scale > widthPx)
    {
        scale = (std::max) (widthPx, 1.0f) / (float) picture.width;
    }

    return scale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRowLayout::GetPictureLines
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextRowLayout::GetPictureLines (const DxuiTextView::Picture & picture, float widthPx, float pxPerDip, int lineHeightPx)
{
    float  height = (float) picture.height * GetPictureScale (picture, widthPx, pxPerDip);



    return (std::max) (1, (int) std::ceil (height / (float) (std::max) (1, lineHeightPx)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextRowLayout::PaintPicture
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextRowLayout::PaintPicture (IDxuiTextRenderer & text, const DxuiTextView::Picture & picture, float left, float top, float widthPx, float pxPerDip)
{
    HRESULT  hr    = S_OK;
    float    scale = GetPictureScale (picture, widthPx, pxPerDip);



    if (picture.width > 0 && picture.height > 0 && picture.bgra.size() >= (size_t) picture.width * (size_t) picture.height)
    {
        hr = text.DrawFramebuffer (picture.bgra.data(), picture.width, picture.height, left, top, (float) picture.width * scale, (float) picture.height * scale);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}
