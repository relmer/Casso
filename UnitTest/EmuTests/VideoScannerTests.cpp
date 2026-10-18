#include "Pch.h"

#include "Machines/Apple2/Common/VideoScanner.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VideoScannerTests
//
//  The scanned address against the values Sather works through: the three
//  40-byte sections of a 128-byte segment, the horizontal-blank address 24
//  bytes before the displayed one, and the HIRES line spacing of 1,024
//  bytes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VideoScannerTests)
{
public:

    static constexpr uint32_t  kLine         = VideoScanner::kCyclesPerScanline;
    static constexpr uint32_t  kFirstVisible = 25;     // H5-H3 = 011
    static constexpr uint32_t  kLastVisible  = 64;

    static uint32_t  At (uint32_t line, uint32_t cycle) { return line * kLine + cycle; }


    TEST_METHOD (TextPageOneRowsAndThirds)
    {
        VideoScanMode  text;

        Assert::AreEqual<Word> (0x0400, VideoScanner::GetScannedAddress (At (0,   kFirstVisible), text));
        Assert::AreEqual<Word> (0x0427, VideoScanner::GetScannedAddress (At (0,   kLastVisible),  text), L"the 40th byte of row 0");
        Assert::AreEqual<Word> (0x0400, VideoScanner::GetScannedAddress (At (7,   kFirstVisible), text), L"eight lines scan one text row");
        Assert::AreEqual<Word> (0x0480, VideoScanner::GetScannedAddress (At (8,   kFirstVisible), text), L"row 1");
        Assert::AreEqual<Word> (0x0428, VideoScanner::GetScannedAddress (At (64,  kFirstVisible), text), L"row 8, the SECOND 40");
        Assert::AreEqual<Word> (0x0450, VideoScanner::GetScannedAddress (At (128, kFirstVisible), text), L"row 16, the THIRD 40");
        Assert::AreEqual<Word> (0x07D0, VideoScanner::GetScannedAddress (At (184, kFirstVisible), text), L"row 23");
    }


    TEST_METHOD (HorizontalBlankScansTwentyFourBytesEarlierAndItsFirstAddressTwice)
    {
        VideoScanMode  text;

        //  $0400 - 24 is outside the 128-byte segment, so it wraps to $0468.
        Assert::AreEqual<Word> (0x0468, VideoScanner::GetScannedAddress (At (0, 0), text));
        Assert::AreEqual<Word> (0x0468, VideoScanner::GetScannedAddress (At (0, 1), text));
        Assert::AreEqual<Word> (0x0469, VideoScanner::GetScannedAddress (At (0, 2), text));
        Assert::AreEqual<Word> (0x047F, VideoScanner::GetScannedAddress (At (0, 24), text));
    }


    TEST_METHOD (VerticalBlankScansEightBytesBeforeTheFirstForty)
    {
        VideoScanMode  text;

        Assert::AreEqual<Word> (0x0478, VideoScanner::GetScannedAddress (At (192, kFirstVisible), text));
        Assert::AreEqual<Word> (0x07F8, VideoScanner::GetScannedAddress (At (256, kFirstVisible), text), L"$FA, the counter's first state after its wrap");
    }


    TEST_METHOD (PageTwoUnless80Store)
    {
        VideoScanMode  page2;
        VideoScanMode  store80;

        page2.page2   = true;
        store80.page2 = true;
        store80.store80 = true;

        Assert::AreEqual<Word> (0x0800, VideoScanner::GetScannedAddress (At (0, kFirstVisible), page2));
        Assert::AreEqual<Word> (0x0400, VideoScanner::GetScannedAddress (At (0, kFirstVisible), store80));
    }


    TEST_METHOD (HiresLinesAreAKilobyteApart)
    {
        VideoScanMode  hires;
        VideoScanMode  hires2;

        hires.graphics = true;
        hires.hires    = true;
        hires2         = hires;
        hires2.page2   = true;

        Assert::AreEqual<Word> (0x2000, VideoScanner::GetScannedAddress (At (0, kFirstVisible), hires));
        Assert::AreEqual<Word> (0x2400, VideoScanner::GetScannedAddress (At (1, kFirstVisible), hires));
        Assert::AreEqual<Word> (0x3C00, VideoScanner::GetScannedAddress (At (7, kFirstVisible), hires));
        Assert::AreEqual<Word> (0x2080, VideoScanner::GetScannedAddress (At (8, kFirstVisible), hires));
        Assert::AreEqual<Word> (0x4000, VideoScanner::GetScannedAddress (At (0, kFirstVisible), hires2));
    }


    TEST_METHOD (MixedScansTheBottomFourRowsAsText)
    {
        VideoScanMode  mixed;

        mixed.graphics = true;
        mixed.hires    = true;
        mixed.mixed    = true;

        Assert::AreEqual<Word> (0x3DD0, VideoScanner::GetScannedAddress (At (159, kFirstVisible), mixed), L"the last graphics line");
        Assert::AreEqual<Word> (0x0650, VideoScanner::GetScannedAddress (At (160, kFirstVisible), mixed), L"row 20, text");
    }


    TEST_METHOD (LoresScansTheTextPage)
    {
        VideoScanMode  lores;

        lores.graphics = true;

        Assert::AreEqual<Word> (0x0480, VideoScanner::GetScannedAddress (At (8, kFirstVisible), lores));
    }
};
