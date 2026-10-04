#include "Pch.h"

#include "Machines/Apple2/Common/VideoScanner.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VideoScannerAddressTests
//
//  The address the video scanner fetches on a cycle of the frame. The
//  expected addresses on displayed cycles are the screen memory maps of the
//  Apple IIe Technical Reference Manual, chapter 2: text row r at
//  $0400 + 128 * (r mod 8) + 40 * (r div 8), hi-res line y at
//  $2000 + 1024 * (y mod 8) + 128 * ((y div 8) mod 8) + 40 * (y div 64).
//  The blanking addresses are Sather's, Understanding the Apple IIe,
//  chapter 5: the same adder run on the blanking counter states, which in
//  horizontal blanking lands on the row's screen holes, $68-$7F.
//
//  A line is 65 cycles; its first 25 are horizontal blanking, so column c
//  is fetched on cycle 25 + c.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VideoScannerAddressTests)
{
public:

    TEST_METHOD (TextColumnsFollowTheManualsMap)
    {
        VideoScanMode  text;

        Assert::AreEqual ((Word) 0x0400, Scan (0,   kFirstColumn,      text), L"row 0, column 0");
        Assert::AreEqual ((Word) 0x0427, Scan (0,   kFirstColumn + 39, text), L"row 0, column 39");
        Assert::AreEqual ((Word) 0x0480, Scan (8,   kFirstColumn,      text), L"row 1");
        Assert::AreEqual ((Word) 0x0428, Scan (64,  kFirstColumn,      text), L"row 8");
        Assert::AreEqual ((Word) 0x0650, Scan (160, kFirstColumn,      text), L"row 20");
        Assert::AreEqual ((Word) 0x07F7, Scan (191, kFirstColumn + 39, text), L"row 23, column 39");
    }


    TEST_METHOD (HiresLinesFollowTheManualsMap)
    {
        VideoScanMode  hires = MakeHires();

        for (uint32_t line = 0; line < kDisplayedLines; line++)
        {
            Word  expected = static_cast<Word> (0x2000 + 0x400 * (line % 8) + 0x80 * ((line / 8) % 8) + 0x28 * (line / 64));

            Assert::AreEqual (expected,                         Scan (line, kFirstColumn,      hires));
            Assert::AreEqual (static_cast<Word> (expected + 39), Scan (line, kFirstColumn + 39, hires));
        }
    }


    TEST_METHOD (HorizontalBlankingFetchesTheScreenHoles)
    {
        VideoScanMode  text;

        Assert::AreEqual ((Word) 0x0468, Scan (0, 0,  text), L"the line's first cycle, counter state $00");
        Assert::AreEqual ((Word) 0x0468, Scan (0, 1,  text), L"state $40 repeats it");
        Assert::AreEqual ((Word) 0x047F, Scan (0, 24, text), L"state $57, the last blanking cycle");
        Assert::AreEqual ((Word) 0x04E8, Scan (8, 1,  text), L"row 1's holes");
    }


    TEST_METHOD (VerticalBlankingRunsTheAdderOn)
    {
        VideoScanMode  text;

        Assert::AreEqual ((Word) 0x0478, Scan (192, kFirstColumn, text), L"the first blanked line, V = $1C0");
        Assert::AreEqual ((Word) 0x07F8, Scan (256, kFirstColumn, text), L"V = $0FA, after the counter wraps");
        Assert::AreEqual ((Word) 0x07F8, Scan (261, kFirstColumn, text), L"V = $0FF, the frame's last line");
    }


    TEST_METHOD (MixedModeShowsTextOnTheBottomFourRows)
    {
        VideoScanMode  mixed = MakeHires();

        mixed.mixed = true;

        Assert::AreEqual ((Word) 0x3DD0, Scan (159, kFirstColumn, mixed), L"line 159 is still hi-res");
        Assert::AreEqual ((Word) 0x0650, Scan (160, kFirstColumn, mixed), L"line 160 is text row 20");
        Assert::AreEqual ((Word) 0x07D0, Scan (191, kFirstColumn, mixed), L"line 191 is text row 23");
    }


    TEST_METHOD (PageTwoMovesBothMaps)
    {
        VideoScanMode  text;
        VideoScanMode  hires = MakeHires();

        text.page2  = true;
        hires.page2 = true;

        Assert::AreEqual ((Word) 0x0800, Scan (0, kFirstColumn, text));
        Assert::AreEqual ((Word) 0x4000, Scan (0, kFirstColumn, hires));
    }


    TEST_METHOD (LoresUsesTheTextMap)
    {
        VideoScanMode  lores;

        lores.graphics = true;

        Assert::AreEqual ((Word) 0x07F7, Scan (191, kFirstColumn + 39, lores));
    }


    TEST_METHOD (TheIIPlusSetsA12InHorizontalBlankingInText)
    {
        VideoScanMode  text;
        VideoScanMode  hires = MakeHires();

        Assert::AreEqual ((Word) 0x1468, VideoScanner::GetScanAddress (0, text, true),  L"text in blanking");
        Assert::AreEqual ((Word) 0x0400, VideoScanner::GetScanAddress (kFirstColumn, text, true), L"not on displayed columns");
        Assert::AreEqual ((Word) 0x2068, VideoScanner::GetScanAddress (0, hires, true), L"nor in hi-res, where A12 is VC");
    }


private:

    static constexpr uint32_t  kFirstColumn    = 25;
    static constexpr uint32_t  kDisplayedLines = 192;
    static constexpr uint32_t  kCyclesPerLine  = 65;

    static Word Scan (uint32_t line, uint32_t cycleInLine, const VideoScanMode & mode)
    {
        return VideoScanner::GetScanAddress (line * kCyclesPerLine + cycleInLine, mode, false);
    }

    static VideoScanMode MakeHires()
    {
        VideoScanMode  mode;

        mode.graphics = true;
        mode.hires    = true;

        return mode;
    }
};
