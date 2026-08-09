#include "Pch.h"

#include "Devices/Printer/PrinterHead.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterHeadTests
//
//  The pure mechanical head math -- every carriage / feed case that has bitten
//  us repeatedly when it lived untested in the exe-side worker. Carriage speed
//  and the right-to-left logic-seek mirror (around the LINE width, not the
//  platen), where the carriage parks through a feed, feed-to-sound form-feed
//  pacing, the monotonic print frontier, and the wet-ink overprint layer. Times
//  are chosen as exact binary fractions (multiples of 1/8 s) so 4000 dots/s and
//  480 rows/s land on exact dot / row counts.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrinterHeadTests
{
    static PrinterEvent Burst (int row, int toDot, InkPrimary color = InkPrimary::Black)
    {
        PrinterEvent   ev;
        ev.type  = PrinterEventType::HeadBurst;
        ev.row   = row;
        ev.toDot = toDot;
        ev.color = color;
        return ev;
    }


    static PrinterEvent Line (int rows)
    {
        PrinterEvent   ev;
        ev.type = PrinterEventType::LineFeed;
        ev.rows = rows;
        return ev;
    }


    static PrinterEvent Form (int rows)
    {
        PrinterEvent   ev;
        ev.type = PrinterEventType::FormFeed;
        ev.rows = rows;
        return ev;
    }


    static void Queue1 (PrinterHead & head, const PrinterEvent & ev)
    {
        vector<PrinterEvent>   v;
        v.push_back (ev);
        head.Queue (v);
    }


    // Run a burst / feed to completion (any leftover time is discarded once the
    // queue empties), for tests that only care about the resulting state.
    static void RunToIdle (PrinterHead & head, const PrintRaster & built, PrintRaster & presented)
    {
        head.Advance (100.0, built, presented);
    }




    TEST_CLASS (PrinterHeadTests)
    {
    public:

        // Parking / idle

        TEST_METHOD (Reset_ParksAtRow)
        {
            PrinterHead   head;

            head.Reset (500);

            Assert::AreEqual (500, head.PlatenRow(),   L"platen parks at the reset row");
            Assert::AreEqual (500, head.RevealTop(),   L"frontier starts at the park row");
            Assert::AreEqual (0,   head.CarriageCol(), L"carriage parks at the left margin");
            Assert::IsTrue   (head.Idle(),  L"a freshly reset head is idle");
            Assert::IsFalse  (head.Moving(), L"a freshly reset head is not moving");
        }


        TEST_METHOD (Idle_NoPending_NoMotion)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            RunToIdle (head, built, presented);   // nothing queued

            Assert::IsTrue   (head.Idle(),  L"stays idle with an empty timeline");
            Assert::AreEqual (0, head.PlatenRow(), L"platen does not drift");
            Assert::AreEqual (0, presented.RowsUsed(), L"nothing painted");
        }


        // Carriage geometry

        TEST_METHOD (SweepLtr_CarriageTracks0ToWidth)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Burst (0, 599));        // width 600, first pass is L->R

            head.Advance (0.125, built, presented);   // 4000 * 0.125 = 500 dots

            Assert::IsTrue   (head.SweepLtr(),        L"first pass sweeps left to right");
            Assert::AreEqual (500, head.CarriageCol(), L"carriage tracks the sweep edge");
            Assert::AreEqual (500, head.MaskCol(),     L"reveal mask tracks the sweep edge");

            RunToIdle (head, built, presented);        // finish the pass
            Assert::AreEqual (600, head.CarriageCol(), L"carriage parks at the line's right edge");
        }


        TEST_METHOD (SweepRtl_MirrorsAroundLineWidthNotPlaten)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Burst (0, 639));        // width 640, L->R
            RunToIdle (head, built, presented);   // completes; next pass flips to R->L

            Queue1 (head, Burst (0, 639));        // width 640, now R->L
            head.Advance (0.125, built, presented);   // headCol = 500

            Assert::IsFalse (head.SweepLtr(), L"second pass sweeps right to left");

            // Mirror around THIS line's width (640 - 500 = 140), not the full
            // platen (1280 - 500 = 780) -- the logic-seek fix that stopped the
            // carriage flying off to the right margin over blank paper.
            Assert::AreEqual (140, head.CarriageCol(), L"R->L carriage mirrors around the line width");
            Assert::IsTrue   (head.CarriageCol() < 700, L"carriage never mirrors around the platen");
        }


        TEST_METHOD (Carriage_StaysWithinLineWidth_BothDirections)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;
            const int     w = 600;

            head.Reset (0);

            Queue1 (head, Burst (0, w - 1));      // L->R
            for (int i = 0; i < 40 && !head.Idle(); i++)
            {
                head.Advance (0.01, built, presented);   // ~40 dots per step
                Assert::IsTrue (head.CarriageCol() >= 0 && head.CarriageCol() <= w,
                                L"L->R carriage stays within [0, width]");
            }

            Queue1 (head, Burst (0, w - 1));      // R->L
            for (int i = 0; i < 40 && !head.Idle(); i++)
            {
                head.Advance (0.01, built, presented);
                Assert::IsTrue (head.CarriageCol() >= 0 && head.CarriageCol() <= w,
                                L"R->L carriage stays within [0, width]");
            }
        }


        TEST_METHOD (Carriage_ParksDuringFeed_NotSnapToZero)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Burst (0, 599));        // width 600, L->R
            RunToIdle (head, built, presented);   // parks the carriage at 600

            Queue1 (head, Line (48));             // a line feed
            head.Advance (0.03125, built, presented);   // mid-feed (480 * 0.03125 = 15 rows)

            Assert::IsTrue   (head.Moving(),           L"paper is feeding");
            Assert::AreEqual (0,   head.MaskCol(),     L"reveal mask closes during a feed");
            Assert::AreEqual (600, head.CarriageCol(), L"carriage parks where the pass ended, not the left margin");
        }


        TEST_METHOD (Direction_AlternatesPerPass)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);

            Queue1 (head, Burst (0, 399));
            head.Advance (0.05, built, presented);
            Assert::IsTrue  (head.SweepLtr(), L"pass 1 is L->R");
            RunToIdle (head, built, presented);

            Queue1 (head, Burst (0, 399));
            head.Advance (0.05, built, presented);
            Assert::IsFalse (head.SweepLtr(), L"pass 2 is R->L");
            RunToIdle (head, built, presented);

            Queue1 (head, Burst (0, 399));
            head.Advance (0.05, built, presented);
            Assert::IsTrue  (head.SweepLtr(), L"pass 3 is L->R again");
        }


        TEST_METHOD (Feed_DoesNotFlipDirection)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Burst (0, 399));        // pass 1: L->R
            RunToIdle (head, built, presented);

            Queue1 (head, Line (48));             // feed between the passes
            RunToIdle (head, built, presented);

            Queue1 (head, Burst (48, 399));       // pass 2: must still be R->L
            head.Advance (0.05, built, presented);

            Assert::IsFalse (head.SweepLtr(), L"a feed does not consume a direction flip");
        }


        // Feed rates

        TEST_METHOD (LineFeed_AtLineRate)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Line (48));
            head.Advance (0.0625, built, presented);   // 480 * 0.0625 = 30 rows

            Assert::AreEqual (30, head.PlatenRow(), L"a line feed slews at 480 rows/s");
            Assert::IsTrue   (head.Moving(), L"still feeding toward the target");
        }


        TEST_METHOD (FormFeed_FullPage_MatchesLongGrain)
        {
            Assert::AreEqual (3.210, PrinterHead::FormFeedDurationSec (PrinterGrid::kPageRows), 0.0001,
                              L"a full-page form feed uses the long sound grain");
        }


        TEST_METHOD (FormFeed_HalfPage_MatchesMediumGrain)
        {
            Assert::AreEqual (1.250, PrinterHead::FormFeedDurationSec (PrinterGrid::kPageRows / 2), 0.0001,
                              L"a half-page form feed uses the medium sound grain");
        }


        TEST_METHOD (FormFeed_Short_MatchesShortGrain)
        {
            Assert::AreEqual (0.680, PrinterHead::FormFeedDurationSec (100), 0.0001,
                              L"a short form feed uses the short sound grain");
        }


        TEST_METHOD (FormFeed_GrainThresholds)
        {
            // Boundaries at 1/3 and 2/3 of a page (528 and 1056 rows).
            Assert::AreEqual (0.680, PrinterHead::FormFeedDurationSec (527),  0.0001, L"just below 1/3 -> short");
            Assert::AreEqual (1.250, PrinterHead::FormFeedDurationSec (528),  0.0001, L"at 1/3 -> medium");
            Assert::AreEqual (1.250, PrinterHead::FormFeedDurationSec (1055), 0.0001, L"just below 2/3 -> medium");
            Assert::AreEqual (3.210, PrinterHead::FormFeedDurationSec (1056), 0.0001, L"at 2/3 -> long");
        }


        TEST_METHOD (FormFeed_PacesToGrainDuration)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Form (100));            // grain 0.680 s -> ~147 rows/s

            head.Advance (0.340, built, presented);   // half the grain -> ~half the rows
            Assert::IsTrue (head.PlatenRow() >= 49 && head.PlatenRow() <= 51,
                            L"form feed paces to the sound grain, not the line rate");

            head.Advance (0.340, built, presented);   // the rest of the grain
            Assert::AreEqual (100, head.PlatenRow(), L"paper stops exactly when the sound ends");
            Assert::IsTrue   (head.Idle(), L"the form feed completes at the grain duration");
        }


        // Frontier / reveal

        TEST_METHOD (Frontier_Monotonic_OverlappingBands)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Burst (0, 99));         // band 1 at row 0 -> frontier 16
            RunToIdle (head, built, presented);
            Assert::AreEqual (PrinterGrid::kPinBandRows, head.RevealTop(),
                              L"frontier advances a full band past the pass row");

            Queue1 (head, Line (14));             // Print Shop feeds 14 into a 16-row band
            RunToIdle (head, built, presented);

            Queue1 (head, Burst (14, 99));        // band 2 at row 14 overlaps rows 14,15
            head.Advance (0.005, built, presented);   // mid-pass

            // The mask holds at the previous frontier (16), NOT at the pass row
            // (14), so the two rows band 1 already printed don't blank as band 2
            // starts -- the monotonic-frontier fix for the overlap blink.
            Assert::AreEqual (16, head.RevealTop(), L"mask never rises above the print frontier");
        }


        // Wet-ink presented layer

        TEST_METHOD (Overprint_SameRow_BuildsComposite)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            // The finished strip carries both primaries at row 0 (Print Shop lays
            // red, then re-strikes blue over it).
            for (int c = 0; c < 100; c++)
            {
                built.Strike (c, 0, InkPrimary::Red);
                built.Strike (c, 0, InkPrimary::Blue);
            }

            head.Reset (0);

            Queue1 (head, Burst (0, 99, InkPrimary::Red));    // pass 1 lays red only
            RunToIdle (head, built, presented);
            Assert::AreEqual ((int) InkPrimary::Red, (int) presented.CellAt (50, 0),
                              L"first pass presents red alone");

            Queue1 (head, Burst (0, 99, InkPrimary::Blue));   // pass 2 overprints blue
            RunToIdle (head, built, presented);
            Assert::AreEqual ((int) InkPrimary::Red | (int) InkPrimary::Blue, (int) presented.CellAt (50, 0),
                              L"overprint accrues into a composite (purple)");
        }


        TEST_METHOD (Sweep_PaintsOnlyUpToHead)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            for (int c = 0; c < PrinterGrid::kDotsPerRow; c++)
            {
                built.Strike (c, 0, InkPrimary::Black);
            }

            head.Reset (0);
            Queue1 (head, Burst (0, PrinterGrid::kDotsPerRow - 1, InkPrimary::Black));   // full-width, L->R
            head.Advance (0.125, built, presented);   // head at col 500

            Assert::AreNotEqual (0, (int) presented.CellAt (400, 0), L"painted behind the head");
            Assert::AreEqual    (0, (int) presented.CellAt (600, 0), L"not yet painted ahead of the head");
        }


        TEST_METHOD (Rtl_PaintsFromRightEdge)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presentedA;   // pass 1 (only used to flip direction)
            PrintRaster   presentedB;   // pass 2's fresh wet-ink layer
            const int     w = 1000;

            for (int c = 0; c < w; c++)
            {
                built.Strike (c, 0, InkPrimary::Black);
            }

            head.Reset (0);
            Queue1 (head, Burst (0, w - 1, InkPrimary::Black));   // L->R
            RunToIdle (head, built, presentedA);

            Queue1 (head, Burst (0, w - 1, InkPrimary::Black));   // R->L
            head.Advance (0.125, built, presentedB);   // headCol 500 -> physical [500, 1000]

            Assert::AreNotEqual (0, (int) presentedB.CellAt (900, 0), L"R->L pass paints in from the right edge");
            Assert::AreEqual    (0, (int) presentedB.CellAt (100, 0), L"the left of the line is not yet reached");
        }


        TEST_METHOD (BlankFormFeed_AdvancesPlaten_NoPaint)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Form (200));
            RunToIdle (head, built, presented);

            Assert::AreEqual (200, head.PlatenRow(), L"a blank form feed slews the platen");
            Assert::AreEqual (0, presented.RowsUsed(), L"but lays no ink");
        }


        // Backpressure accounting

        TEST_METHOD (PendingSeconds_SumsQueuedMotion)
        {
            PrinterHead   head;

            head.Reset (0);
            Queue1 (head, Burst (0, PrinterGrid::kDotsPerRow - 1));   // 1280 / 4000 = 0.320 s
            Queue1 (head, Line (14));                                 //   14 /  480 = 0.0292 s

            Assert::AreEqual (0.32 + 14.0 / 480.0, head.PendingSeconds(), 0.001,
                              L"queued print time sums each event at its own speed");
        }


        TEST_METHOD (PendingSeconds_DrainsAsHeadAdvances)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);
            Queue1 (head, Burst (0, PrinterGrid::kDotsPerRow - 1));   // 0.320 s of sweep

            Assert::AreEqual (0.320, head.PendingSeconds(), 0.001, L"full sweep queued");

            head.Advance (0.1, built, presented);   // 400 dots consumed
            Assert::AreEqual (0.220, head.PendingSeconds(), 0.001, L"queued time drains as the head sweeps");
        }


        TEST_METHOD (LargeTimeStep_ProcessesMultipleBursts_NoSkip)
        {
            PrinterHead   head;
            PrintRaster   built;
            PrintRaster   presented;

            head.Reset (0);

            // Five bands with feeds between -- more motion than one preview frame
            // can hold; a single large tick must replay all of it, not skip ahead.
            for (int band = 0; band < 5; band++)
            {
                Queue1 (head, Burst (band * 16, 99));
                Queue1 (head, Line (16));
            }

            head.Advance (100.0, built, presented);

            Assert::IsTrue   (head.Idle(), L"the whole timeline is consumed in one tick");
            Assert::AreEqual (5 * 16, head.PlatenRow(), L"the platen reaches the last feed, none skipped");
        }
    };
}
