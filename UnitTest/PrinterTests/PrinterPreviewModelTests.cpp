#include "Pch.h"

#include "Devices/Printer/PrinterPreviewModel.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/PrinterTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPreviewModelTests
//
//  The pure preview decisions the panel's RefreshLive used to compute inline --
//  the reveal-mask column span, the tear-off / freeze-heal / catch-up gates, and
//  the audio sample window with its line-wrap detection. These are the exact
//  cases that kept regressing while they lived untested in the exe.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrinterPreviewModelTests
{
    TEST_CLASS (PrinterPreviewModelTests)
    {
    public:

        // Reveal-mask column span

        TEST_METHOD (RevealColumnSpan_LeftToRight)
        {
            PrinterPreviewModel::RevealSpan   rs = PrinterPreviewModel::RevealColumnSpan (true, 500);

            Assert::AreEqual (0,   rs.loDots, L"L->R reveals from the left margin");
            Assert::AreEqual (500, rs.hiDots, L"...up to the carriage");
        }


        TEST_METHOD (RevealColumnSpan_RightToLeft)
        {
            PrinterPreviewModel::RevealSpan   rs = PrinterPreviewModel::RevealColumnSpan (false, 500);

            Assert::AreEqual (500, rs.loDots, L"R->L reveals from the carriage");
            Assert::AreEqual (PrinterGrid::kDotsPerRow, rs.hiDots, L"...to the right margin");
        }


        // Tear-off

        TEST_METHOD (StripTornOff_WhenStripShrinksBelowLiveRow)
        {
            Assert::IsTrue  (PrinterPreviewModel::StripTornOff (10, 20), L"a shrunk strip is a tear-off");
            Assert::IsFalse (PrinterPreviewModel::StripTornOff (30, 20), L"a growing strip is not");
        }


        // Catch-up gate (the CATALOG buzz fix)

        TEST_METHOD (LiveBandOutsideSpan_BelowAndAbove)
        {
            // Band well below the snapshot span -> outside (fast text catch-up).
            Assert::IsTrue  (PrinterPreviewModel::IsLiveBandOutsideSpan (100, 0, 50),
                             L"a band past the span bottom is outside");
            // Band above the span top -> outside.
            Assert::IsTrue  (PrinterPreviewModel::IsLiveBandOutsideSpan (5, 10, 60),
                             L"a band above the span top is outside");
            // Band fully within the span -> inside.
            Assert::IsFalse (PrinterPreviewModel::IsLiveBandOutsideSpan (10, 0, 60),
                             L"a band inside the span is not outside");
        }


        // Freeze-heal dirty row (the disk-picker corruption fix)

        TEST_METHOD (DirtyFromRow_FirstRenderIsEverything)
        {
            Assert::AreEqual (-1, PrinterPreviewModel::GetDirtyFromRow (false, 100, 999),
                              L"the first render marks everything dirty");
        }


        TEST_METHOD (DirtyFromRow_NormalUsesFixedWindow)
        {
            // Rendered platen close behind: the fixed window (platen - 3 bands) wins.
            int   expected = 100 - 3 * PrinterGrid::kPinBandRows;   // 52

            Assert::AreEqual (expected, PrinterPreviewModel::GetDirtyFromRow (true, 100, 95),
                              L"a fresh render dirties a fixed window at the platen");
        }


        TEST_METHOD (DirtyFromRow_FreezeHealsFromRenderedPlaten)
        {
            // Rendered platen far behind (a multi-second UI freeze): dirty from the
            // last-rendered platen so the frozen-through rows are refreshed.
            int   expected = 30 - PrinterGrid::kPinBandRows;   // 14

            Assert::AreEqual (expected, PrinterPreviewModel::GetDirtyFromRow (true, 100, 30),
                              L"a stale render heals from the last-rendered platen");
        }


        // Change detection

        TEST_METHOD (SpanAndRevealMovedDetectChange)
        {
            Assert::IsFalse (PrinterPreviewModel::HasSpanMoved (0, 50, 0, 50), L"identical span did not move");
            Assert::IsTrue  (PrinterPreviewModel::HasSpanMoved (0, 50, 1, 50), L"a shifted span moved");
            Assert::IsFalse (PrinterPreviewModel::RevealMoved (5, 100, 5, 100), L"identical reveal did not move");
            Assert::IsTrue  (PrinterPreviewModel::RevealMoved (5, 120, 5, 100), L"a swept column moved");
        }


        // Audio sample window

        TEST_METHOD (AudioSampleWindow_LtrBridgesBehindHead)
        {
            // L->R word: sample from a bridge behind the trailing column up to the
            // leading column, so a word buzzes across its inter-glyph gaps.
            PrinterPreviewModel::InkSample   s = PrinterPreviewModel::GetAudioSampleWindow (true, 100, 200, 5, 5);

            constexpr int   kBridge = (PrinterGrid::kDotsPerInchH * 3) / 20;   // 24

            Assert::AreEqual (100 - kBridge, s.loCol, L"the bridge trails the head");
            Assert::AreEqual (200,           s.hiCol, L"...up to the leading column");
        }


        TEST_METHOD (AudioSampleWindow_RtlBridgesAhead)
        {
            PrinterPreviewModel::InkSample   s = PrinterPreviewModel::GetAudioSampleWindow (false, 200, 100, 5, 5);

            constexpr int   kBridge = (PrinterGrid::kDotsPerInchH * 3) / 20;

            Assert::AreEqual (100,           s.loCol, L"R->L samples from the leading column");
            Assert::AreEqual (200 + kBridge, s.hiCol, L"...with the bridge trailing to the right");
        }


        TEST_METHOD (AudioSampleWindow_LineWrapSamplesWholeRow)
        {
            // A margin-to-margin column jump (line wrap) has no contiguous span, so
            // the whole row is sampled -- any ink means a printing pass.
            PrinterPreviewModel::InkSample   s = PrinterPreviewModel::GetAudioSampleWindow (true, 1200, 10, 6, 5);

            Assert::AreEqual (0, s.loCol, L"a wrap samples the whole row");
            Assert::AreEqual (PrinterGrid::kDotsPerRow - 1, s.hiCol, L"...to the right margin");
        }


        TEST_METHOD (AudioSampleWindow_ClampsToBounds)
        {
            // A window near the left margin must clamp its bridge to 0.
            PrinterPreviewModel::InkSample   s = PrinterPreviewModel::GetAudioSampleWindow (true, 10, 10, 5, 5);

            Assert::AreEqual (0,  s.loCol, L"the bridge clamps to the left margin");
            Assert::AreEqual (10, s.hiCol, L"the leading column is kept");
        }


        // Ink probe

        TEST_METHOD (BandHasInk_FindsInkInRangeOnly)
        {
            PrintRaster   span;

            span.Strike (50, 2, InkPrimary::Black);   // span-relative row 2, col 50

            Assert::IsTrue  (PrinterPreviewModel::HasBandInk (span, 0, 0, 40, 60),
                             L"ink within the band and column range is found");
            Assert::IsFalse (PrinterPreviewModel::HasBandInk (span, 0, 0, 0, 30),
                             L"ink outside the column range is not");
        }


        TEST_METHOD (BandHasInk_RebasesBySpanFirstRow)
        {
            PrintRaster   span;

            span.Strike (50, 2, InkPrimary::Black);   // span-relative row 2

            // Absolute reveal row 5 with the span starting at absolute row 3 maps to
            // span-relative row 2 -- so the band [2..17] covers the struck cell.
            Assert::IsTrue (PrinterPreviewModel::HasBandInk (span, 3, 5, 40, 60),
                            L"the reveal row is rebased by the span's first row");
        }
    };
}
