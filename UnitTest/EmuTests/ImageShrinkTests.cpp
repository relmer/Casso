#include "Pch.h"

#include "Video/ImageShrink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageShrinkTests
//
//  The box filter halves the emulator's picture to a machine state's
//  thumbnail size, each output pixel the rounded average of the four it
//  covers, channel by channel.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageShrinkTests)
{
public:

    TEST_METHOD (HalvingAveragesEachTwoByTwoBlock)
    {
        constexpr int       kSourceWidth  = 560;
        constexpr int       kSourceHeight = 384;
        constexpr int       kThumbWidth   = 280;
        constexpr int       kThumbHeight  = 192;
        constexpr uint32_t  kDark         = 0xFF102030;
        constexpr uint32_t  kLight        = 0xFF305070;
        constexpr uint32_t  kAverage      = 0xFF203850;
        DxuiIconImage       source;
        DxuiIconImage       thumb;
        int                 x             = 0;
        int                 y             = 0;



        source.width  = kSourceWidth;
        source.height = kSourceHeight;
        source.bgraPremul.resize ((size_t) kSourceWidth * (size_t) kSourceHeight);

        // Each two-by-two block has two dark and two light pixels, on its
        // diagonals, so every block averages to the same value.
        for (y = 0; y < kSourceHeight; y++)
        {
            for (x = 0; x < kSourceWidth; x++)
            {
                source.bgraPremul[(size_t) y * kSourceWidth + x] = ((x + y) % 2 == 0) ? kDark : kLight;
            }
        }

        ImageShrink::Shrink (source, kThumbWidth, kThumbHeight, thumb);

        Assert::AreEqual (kThumbWidth,  thumb.width,  L"width");
        Assert::AreEqual (kThumbHeight, thumb.height, L"height");

        for (uint32_t pixel : thumb.bgraPremul)
        {
            Assert::AreEqual (kAverage, pixel, L"every pixel is its block's average");
        }
    }
};





