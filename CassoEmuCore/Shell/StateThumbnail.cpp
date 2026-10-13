#include "Pch.h"

#include "Shell/StateThumbnail.h"

#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/RgbaImage.h"
#include "Video/ImageShrink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateThumbnail::Encode
//
//  framebuffer is width by height pixels of 0xAARRGGBB, top row first, as
//  the renderer publishes it. The thumbnail is opaque whatever the alpha in
//  the framebuffer. WIC encodes the PNG, so the calling thread must have COM
//  initialized.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StateThumbnail::Encode (
    const std::vector<uint32_t>  & framebuffer,
    int                            width,
    int                            height,
    std::vector<Byte>            & outPng)
{
    constexpr int     kDpi      = 96;
    constexpr size_t  kChannels = 4;
    constexpr Byte    kOpaque   = 0xFF;
    HRESULT           hr        = S_OK;
    DxuiIconImage     source;
    DxuiIconImage     thumb;
    RgbaImage         rgba;
    bool              isWhole   = width > 0 && height > 0 && framebuffer.size() == (size_t) width * (size_t) height;
    size_t            i         = 0;
    uint32_t          pixel     = 0;



    outPng.clear();

    CBRAEx (isWhole, E_INVALIDARG);

    source.width      = width;
    source.height     = height;
    source.bgraPremul = framebuffer;

    ImageShrink::Shrink (source, kWidth, kHeight, thumb);

    rgba.width  = thumb.width;
    rgba.height = thumb.height;
    rgba.rgba.resize (thumb.bgraPremul.size() * kChannels);

    for (i = 0; i < thumb.bgraPremul.size(); i++)
    {
        pixel = thumb.bgraPremul[i];

        rgba.rgba[i * kChannels + 0] = (Byte) (pixel >> (CHAR_BIT * 2));
        rgba.rgba[i * kChannels + 1] = (Byte) (pixel >> CHAR_BIT);
        rgba.rgba[i * kChannels + 2] = (Byte) pixel;
        rgba.rgba[i * kChannels + 3] = kOpaque;
    }

    hr = PngCodec::EncodeRgba (rgba, kDpi, outPng);
    CHR (hr);

Error:
    return hr;
}





