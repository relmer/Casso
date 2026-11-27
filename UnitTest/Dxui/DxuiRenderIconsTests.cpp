#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderIconsTests
//
//  The box filter a frame drawn smaller than its pixels is averaged with.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiRenderIconsTests)
{
public:

    TEST_METHOD (Shrink_AveragesStripesToTheirMean)
    {
        std::vector<uint32_t>  stripes (8 * 2);
        std::vector<uint32_t>  out;
        int                    width   = 0;
        int                    height  = 0;



        for (size_t i = 0; i < stripes.size(); i++)
        {
            stripes[i] = (i % 2 == 0) ? 0xFFFF0000u : 0xFF0000FFu;
        }

        DxuiRenderIcons::Shrink (stripes.data(), 8, 2, 4, 1, out, width, height);

        Assert::AreEqual (4, width);
        Assert::AreEqual (1, height);

        for (uint32_t pixel : out)
        {
            Assert::AreEqual (0xFF800080u, pixel, L"red and blue stripes average to purple");
        }
    }


    TEST_METHOD (Shrink_KeepsASideNotShrunk)
    {
        std::vector<uint32_t>  pixels (4 * 3, 0xFF336699u);
        std::vector<uint32_t>  out;
        int                    width  = 0;
        int                    height = 0;



        DxuiRenderIcons::Shrink (pixels.data(), 4, 3, 2, 10, out, width, height);

        Assert::AreEqual (2,           width);
        Assert::AreEqual (3,           height);
        Assert::AreEqual (0xFF336699u, out[5]);
    }
};
