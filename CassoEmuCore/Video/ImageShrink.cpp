#include "Pch.h"

#include "Video/ImageShrink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageShrink::Shrink
//
//  A box filter: every destination pixel averages the source pixels whose
//  centers fall in its share of the picture, channel by channel.
//
////////////////////////////////////////////////////////////////////////////////

void ImageShrink::Shrink (
    const DxuiIconImage  & source,
    int                    width,
    int                    height,
    DxuiIconImage        & outImage)
{
    constexpr int  kChannels = 4;



    int       dx             = 0;
    int       dy             = 0;
    int       sx             = 0;
    int       sy             = 0;
    int       x0             = 0;
    int       x1             = 0;
    int       y0             = 0;
    int       y1             = 0;
    int       c              = 0;
    uint64_t  sum[kChannels] = {};
    uint64_t  n              = 0;
    uint32_t  pixel          = 0;
    uint32_t  out            = 0;



    outImage.width  = (std::max) (width, 0);
    outImage.height = (std::max) (height, 0);
    outImage.bgraPremul.assign ((size_t) outImage.width * (size_t) outImage.height, 0);

    if (source.width <= 0 || source.height <= 0)
    {
        return;
    }

    for (dy = 0; dy < outImage.height; dy++)
    {
        y0 = dy * source.height / outImage.height;
        y1 = (std::max) (y0 + 1, (dy + 1) * source.height / outImage.height);

        for (dx = 0; dx < outImage.width; dx++)
        {
            x0 = dx * source.width / outImage.width;
            x1 = (std::max) (x0 + 1, (dx + 1) * source.width / outImage.width);
            n   = 0;
            out = 0;

            std::fill (std::begin (sum), std::end (sum), 0);

            for (sy = y0; sy < y1; sy++)
            {
                for (sx = x0; sx < x1; sx++)
                {
                    pixel = source.bgraPremul[(size_t) sy * (size_t) source.width + (size_t) sx];

                    for (c = 0; c < kChannels; c++)
                    {
                        sum[c] += (pixel >> (CHAR_BIT * c)) & 0xFFu;
                    }

                    n++;
                }
            }

            for (c = 0; c < kChannels; c++)
            {
                out |= (uint32_t) ((sum[c] + n / 2) / n) << (CHAR_BIT * c);
            }

            outImage.bgraPremul[(size_t) dy * (size_t) outImage.width + (size_t) dx] = out;
        }
    }
}





