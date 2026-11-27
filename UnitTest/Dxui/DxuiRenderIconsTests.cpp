#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderIconsTests
//
//  The resampling of a frame drawn at anything but a whole multiple of its
//  size.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiRenderIconsTests)
{
public:

    static std::vector<uint32_t>  MakeStripes (int width, int height)
    {
        std::vector<uint32_t>  stripes ((size_t) width * height);



        for (size_t i = 0; i < stripes.size(); i++)
        {
            stripes[i] = ((i % (size_t) width) % 2 == 0) ? 0xFFFF0000u : 0xFF0000FFu;
        }

        return stripes;
    }


    TEST_METHOD (Resample_HalfSize_AveragesStripesToPurple)
    {
        std::vector<uint32_t>  stripes = MakeStripes (8, 2);
        std::vector<uint32_t>  out;



        DxuiRenderIcons::Resample (stripes.data(), 8, 2, 4, 1, true, false, out);

        Assert::AreEqual ((size_t) 4, out.size());

        for (uint32_t pixel : out)
        {
            Assert::AreEqual (0xFF800080u, pixel, L"red and blue stripes average to purple");
        }
    }


    TEST_METHOD (Resample_NearlyFullSize_BlendsEveryPixel)
    {
        std::vector<uint32_t>  stripes = MakeStripes (40, 1);
        std::vector<uint32_t>  out;



        DxuiRenderIcons::Resample (stripes.data(), 40, 1, 38, 1, true, false, out);

        for (size_t i = 1; i + 1 < out.size(); i++)
        {
            uint32_t  red  = (out[i] >> 16) & 0xFF;
            uint32_t  blue = out[i] & 0xFF;

            Assert::IsTrue (red > 0x60 && red < 0xA0 && blue > 0x60 && blue < 0xA0, L"each pixel mixes both stripes, so no bands form");
        }
    }


    TEST_METHOD (Resample_WithoutBlend_CopiesAWholeMultiple)
    {
        std::vector<uint32_t>  stripes = MakeStripes (4, 1);
        std::vector<uint32_t>  out;



        DxuiRenderIcons::Resample (stripes.data(), 4, 1, 8, 1, false, false, out);

        Assert::AreEqual (0xFFFF0000u, out[0]);
        Assert::AreEqual (0xFFFF0000u, out[1]);
        Assert::AreEqual (0xFF0000FFu, out[2]);
    }


    TEST_METHOD (IsWholeMultiple_AllowsAPixelOfRounding)
    {
        Assert::IsTrue  (DxuiRenderIcons::IsWholeMultiple (640, 640));
        Assert::IsTrue  (DxuiRenderIcons::IsWholeMultiple (1281, 640));
        Assert::IsFalse (DxuiRenderIcons::IsWholeMultiple (610, 640));
        Assert::IsFalse (DxuiRenderIcons::IsWholeMultiple (960, 640));
    }
};
