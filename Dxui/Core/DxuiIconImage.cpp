#include "Pch.h"

#include "DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiIconImage::FromHicon
//
//  Into a top-down 32-bit DIB cleared to zero, with DrawIconEx.
//
//  THE RESULT IS ALREADY PREMULTIPLIED. GDI composites an alpha icon over the
//  cleared bitmap, and compositing over zero produces color times alpha.
//  Measured 2026-09-12 on three icons: none of 103 partly transparent pixels
//  had a color channel above its alpha, which straight alpha would produce at
//  every light-colored edge. A caption that multiplied by alpha a
//  second time darkened the anti-aliased edges of its icon.
//
//  An icon with no alpha channel draws its colors with every alpha at zero;
//  its mask then indicates which pixels are opaque.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiIconImage::FromHicon (HICON icon, int sizePx, DxuiIconImage & outImage)
{
    HRESULT      hr       = S_OK;
    HDC          dc       = nullptr;
    HBITMAP      dib      = nullptr;
    HGDIOBJ      previous = nullptr;
    void       * raw      = nullptr;
    uint32_t   * bits     = nullptr;
    BITMAPINFO   bmi      = {};
    BOOL         drawn    = FALSE;
    bool         anyAlpha = false;
    size_t       count    = 0;
    HRESULT      hrGle    = E_FAIL;



    CBRAEx (icon != nullptr && sizePx > 0, E_INVALIDARG);

    count = (size_t) sizePx * (size_t) sizePx;
    dc    = CreateCompatibleDC (nullptr);
    CPR (dc);

    bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = sizePx;
    bmi.bmiHeader.biHeight      = -sizePx;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    dib = CreateDIBSection (dc, &bmi, DIB_RGB_COLORS, &raw, nullptr, 0);
    CPR (dib);
    CPR (raw);

    bits     = (uint32_t *) raw;
    previous = SelectObject (dc, dib);

    std::fill (bits, bits + count, 0u);

    drawn = DrawIconEx (dc, 0, 0, icon, sizePx, sizePx, 0, nullptr, DI_NORMAL);

    if (!drawn)
    {
        hrGle = HRESULT_FROM_WIN32 (GetLastError());
    }

    CBREx (drawn, hrGle);

    outImage.width  = sizePx;
    outImage.height = sizePx;
    outImage.bgraPremul.assign (bits, bits + count);

    for (uint32_t pixel : outImage.bgraPremul)
    {
        if ((pixel >> 24) != 0)
        {
            anyAlpha = true;
            break;
        }
    }

    if (!anyAlpha)
    {
        hr = ApplyMask (dc, icon, sizePx, bits, outImage);
        CHR (hr);
    }

Error:
    if (previous != nullptr)
    {
        SelectObject (dc, previous);
    }

    if (dib != nullptr)
    {
        DeleteObject (dib);
    }

    if (dc != nullptr)
    {
        DeleteDC (dc);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiIconImage::ApplyMask
//
//  DI_MASK draws black for opaque pixels and white for transparent ones. An
//  opaque pixel's color is already its premultiplied color.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiIconImage::ApplyMask (HDC dc, HICON icon, int sizePx, uint32_t * bits, DxuiIconImage & image)
{
    HRESULT  hr    = S_OK;
    size_t   count = (size_t) sizePx * (size_t) sizePx;
    BOOL     drawn = FALSE;
    HRESULT  hrGle = E_FAIL;



    std::fill (bits, bits + count, 0u);

    drawn = DrawIconEx (dc, 0, 0, icon, sizePx, sizePx, 0, nullptr, DI_MASK);

    if (!drawn)
    {
        hrGle = HRESULT_FROM_WIN32 (GetLastError());
    }

    CBREx (drawn, hrGle);

    for (size_t i = 0; i < count; i++)
    {
        bool  opaque = (bits[i] & 0x00FFFFFFu) == 0;

        image.bgraPremul[i] = opaque ? (image.bgraPremul[i] | 0xFF000000u) : 0u;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiIconImage::Resample
//
//  Premultiplied pixels filter correctly as they are: a transparent pixel adds
//  no color to its neighbors.
//
////////////////////////////////////////////////////////////////////////////////

DxuiIconImage DxuiIconImage::Resample (const DxuiIconImage & source, int sizePx)
{
    DxuiIconImage  out;
    float          scaleX = (sizePx > 0) ? (float) source.width  / (float) sizePx : 0.0f;
    float          scaleY = (sizePx > 0) ? (float) source.height / (float) sizePx : 0.0f;
    bool           usable = sizePx > 0 && source.width > 0 && source.height > 0
                         && source.bgraPremul.size() == (size_t) source.width * (size_t) source.height;



    if (!usable)
    {
        return out;
    }

    out.width  = sizePx;
    out.height = sizePx;
    out.bgraPremul.assign ((size_t) sizePx * (size_t) sizePx, 0u);

    for (int y = 0; y < sizePx; y++)
    {
        float  sy = (std::clamp) (((float) y + 0.5f) * scaleY - 0.5f, 0.0f, (float) (source.height - 1));
        int    y0 = (int) sy;
        int    y1 = (std::min) (y0 + 1, source.height - 1);
        float  fy = sy - (float) y0;

        for (int x = 0; x < sizePx; x++)
        {
            float     sx    = (std::clamp) (((float) x + 0.5f) * scaleX - 0.5f, 0.0f, (float) (source.width - 1));
            int       x0    = (int) sx;
            int       x1    = (std::min) (x0 + 1, source.width - 1);
            float     fx    = sx - (float) x0;
            uint32_t  a     = source.bgraPremul[(size_t) y0 * source.width + x0];
            uint32_t  b     = source.bgraPremul[(size_t) y0 * source.width + x1];
            uint32_t  c     = source.bgraPremul[(size_t) y1 * source.width + x0];
            uint32_t  d     = source.bgraPremul[(size_t) y1 * source.width + x1];
            uint32_t  pixel = 0;

            for (int shift = 0; shift < 32; shift += 8)
            {
                float  top    = (float) ((a >> shift) & 0xFF) * (1.0f - fx) + (float) ((b >> shift) & 0xFF) * fx;
                float  bottom = (float) ((c >> shift) & 0xFF) * (1.0f - fx) + (float) ((d >> shift) & 0xFF) * fx;
                float  value  = top * (1.0f - fy) + bottom * fy;

                pixel |= (uint32_t) (std::min) (255.0f, value + 0.5f) << shift;
            }

            out.bgraPremul[(size_t) y * sizePx + x] = pixel;
        }
    }

    return out;
}
