#include "Pch.h"

#include "Video/BeamOverlay.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BeamOverlayDrawTests
//
//  The debugger's beam marker on a 560 x 384 frame: two rows a scanline,
//  14 dots a cycle.
//
////////////////////////////////////////////////////////////////////////////////

namespace EmuTests
{
    TEST_CLASS (BeamOverlayDrawTests)
    {
    public:

        static constexpr int       kWidth  = 560;
        static constexpr int       kHeight = 384;
        static constexpr uint32_t  kBlack  = 0xFF000000;
        static constexpr uint32_t  kMark   = 0xFFFF0000;

        static uint32_t At (const std::vector<uint32_t> & pixels, int x, int y)
        {
            return pixels[(size_t) y * kWidth + (size_t) x];
        }



        TEST_METHOD (TheBeamsCycleIsASolidBlockOnItsScanline)
        {
            std::vector<uint32_t>  pixels (kWidth * kHeight, kBlack);



            // Scanline 10, cycle 27: the third visible cycle, dots 28-41, rows 20-21,
            // with the bar reaching three scanlines each way, rows 14-27.
            BeamOverlay::Draw (pixels.data(), kWidth, kHeight, 10, 27, kMark);

            Assert::AreEqual (kMark, At (pixels, 28, 20));
            Assert::AreEqual (kMark, At (pixels, 41, 21));
            Assert::AreEqual (kMark, At (pixels, 28, 14));
            Assert::AreEqual (kMark, At (pixels, 41, 27));
            Assert::AreEqual (kBlack, At (pixels, 28, 13), L"the bar ends three scanlines up");
            Assert::AreEqual (kBlack, At (pixels, 28, 28), L"and three down");
            Assert::AreEqual (kBlack, At (pixels, 42, 14), L"the bar is one cycle wide");
            Assert::AreEqual (BeamOverlay::Blend (kBlack, kMark), At (pixels, 27,  20), L"the rest of the line is at half strength");
            Assert::AreEqual (BeamOverlay::Blend (kBlack, kMark), At (pixels, 559, 21));
            Assert::AreEqual (kBlack, At (pixels, 100, 19), L"the line above is untouched");
            Assert::AreEqual (kBlack, At (pixels, 100, 22), L"the line below is untouched");
        }



        TEST_METHOD (HorizontalBlankMarksTheLeftEdge)
        {
            std::vector<uint32_t>  pixels (kWidth * kHeight, kBlack);



            BeamOverlay::Draw (pixels.data(), kWidth, kHeight, 0, 3, kMark);

            Assert::AreEqual (kMark, At (pixels, 0, 0));
            Assert::AreEqual (kMark, At (pixels, 1, 1));
            Assert::AreEqual (BeamOverlay::Blend (kBlack, kMark), At (pixels, 2, 0));
        }



        TEST_METHOD (VerticalBlankMarksTheBottomEdge)
        {
            std::vector<uint32_t>  pixels (kWidth * kHeight, kBlack);



            BeamOverlay::Draw (pixels.data(), kWidth, kHeight, 200, 30, kMark);

            Assert::AreEqual (BeamOverlay::Blend (kBlack, kMark), At (pixels, 100, kHeight - 1));
            Assert::AreEqual (BeamOverlay::Blend (kBlack, kMark), At (pixels, 100, kHeight - 2));
            Assert::AreEqual (kBlack, At (pixels, 100, kHeight - 3));
            Assert::AreEqual (kBlack, At (pixels, 100, 0));
        }



        TEST_METHOD (BlendIsHalfOfEachAndOpaque)
        {
            Assert::AreEqual (0xFF7F7F7Fu, BeamOverlay::Blend (0xFFFFFFFF, 0xFF000000));
            Assert::AreEqual (0xFF7F4020u, BeamOverlay::Blend (0x00FF0000, 0x00008040));
        }
    };
}
