#include "Pch.h"

#include "Devices/Printer/PrinterPacing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Clock-driven pacing for the printer panel's paper animation (R-012). The
// reveal is a single carriage clock: the head sweeps the live band at
// dotsPerSecond, and each completed full-width pass reveals ONE pin band
// (rowsPerSweep rows) and steps down -- so revealing rows and the head sweep are
// the same motion. Rates/thresholds are set explicitly per test so behavior --
// not the tuned default constants -- is what is asserted. kDotsPerRow == 1280;
// tests use dotsPerSecond == 1280 so exactly one sweep completes per second.
namespace PrinterPacingTests
{
    static PrinterPacing::Config Cfg (double dotsPerSec, int rowsPerSweep, double coalesce)
    {
        PrinterPacing::Config   c;
        c.dotsPerSecond          = dotsPerSec;
        c.rowsPerSweep           = rowsPerSweep;
        c.coalesceRows           = coalesce;
        c.resumeThresholdSeconds = 1.0e9;   // math tests inject large discrete steps; don't treat them as resumes
        return c;
    }


    TEST_CLASS (PrinterPacingTests)
    {
    public:

        TEST_METHOD (RevealsOneBandPerSweep)
        {
            // 1280 dots/s over a 1280-dot row == one sweep (one 16-row band) per s.
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (1000);

            Assert::AreEqual (0,  p.Advance (0.5));   // half a pass: no band yet
            Assert::AreEqual (16, p.Advance (1.0));   // one pass done -> 1 band
            Assert::AreEqual (32, p.Advance (2.0));   // two passes -> 2 bands
            Assert::IsFalse  (p.IsCaughtUp());
        }


        TEST_METHOD (ColumnSweepsAcrossTheLiveBand)
        {
            // While behind the guest, the head column is the visible sweep -- not
            // a full-width snap (that was the old bug). 1280 dots/s.
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (1000);

            p.Advance (0.25);
            Assert::AreEqual (320, p.RevealedColDots());   // 1280 * 0.25
            p.Advance (0.50);
            Assert::AreEqual (640, p.RevealedColDots());
            Assert::IsFalse  (p.IsCaughtUp());
        }


        TEST_METHOD (CaughtUpParksTheCarriageAtTheMargin)
        {
            // One full L->R pass reveals the band and catches the guest. The head
            // must PARK where the pass ended (col wraps to 0, direction flips to
            // R->L -- so the presenter rests it at the right margin), NOT snap to
            // full width. Snapping to full with the flipped direction is what
            // teleported the glyph to the far edge (the old bug).
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (16);

            p.Advance (1.0);                           // one pass reveals the band
            Assert::AreEqual (16, p.RevealedRows());
            Assert::IsTrue   (p.IsCaughtUp());
            Assert::AreEqual (0, p.RevealedColDots());     // parked at the pass-end margin
            Assert::IsFalse  (p.SweepLeftToRight());        // reversed for the next pass
        }


        TEST_METHOD (ParkedCarriageHoldsStillWhileCaughtUp)
        {
            // With no new content, further Advances must not move the head at all.
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (16);
            p.Advance (1.0);                           // caught up, parked

            int   parked = p.RevealedColDots();
            p.Advance (2.0);
            Assert::AreEqual (parked, p.RevealedColDots());
            p.Advance (9.0);
            Assert::AreEqual (parked, p.RevealedColDots());
            Assert::IsFalse  (p.SweepLeftToRight());        // no spurious flip either
        }


        TEST_METHOD (ResumesFromTheMarginWithoutBacktrack)
        {
            // The head jog (forward-halfway, back, forward) came from the column
            // snapping to full on catch-up and resetting to zero on the next fed
            // chunk. Here: sweep a full L->R band to catch-up (parks at col 0,
            // now sweeping R->L), then feed more -- the carriage must resume the
            // R->L pass FROM the margin (col climbs 0 -> up), same direction, no
            // reset, no jump.
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (16);
            p.Advance (1.0);                           // pass 1 done, caught up
            Assert::AreEqual (0, p.RevealedColDots());
            Assert::IsFalse  (p.SweepLeftToRight());

            p.SetTargetRows (1000);                    // guest feeds the next band
            Assert::AreEqual (0, p.RevealedColDots());  // feeding alone never moves the head

            Assert::AreEqual (16,  p.Advance (1.25));   // rows unchanged this sub-pass...
            Assert::AreEqual (320, p.RevealedColDots()); // ...head climbed 0 -> 320 off the margin
            Assert::IsFalse  (p.SweepLeftToRight());     // still the same R->L pass
            p.Advance (1.5);
            Assert::AreEqual (640, p.RevealedColDots()); // and keeps climbing, monotonic
        }


        TEST_METHOD (RevealCapsAtTarget)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (40);                      // 40 rows = 2.5 bands

            Assert::AreEqual (16, p.Advance (1.0));
            Assert::AreEqual (32, p.Advance (2.0));
            Assert::AreEqual (40, p.Advance (3.0));    // 3rd band clamps to the target
            Assert::AreEqual (40, p.Advance (9.0));    // stays put
            Assert::IsTrue   (p.IsCaughtUp());
        }


        TEST_METHOD (FastForwardRevealsEverythingNextAdvance)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (5000);
            p.RequestFastForward();

            Assert::AreEqual (5000, p.Advance (0.01));
            Assert::IsTrue   (p.IsCaughtUp());
            Assert::AreEqual (0, p.RevealedColDots());   // jump-cut rests the carriage at home
        }


        TEST_METHOD (FastForwardIsOneShot)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (200);
            p.RequestFastForward();

            Assert::AreEqual (200, p.Advance (0.01));

            // Grow the job; the earlier fast-forward must not still be latched.
            p.SetTargetRows (400);
            Assert::AreEqual (200, p.Advance (0.02));   // sweeping again, not snapped
            Assert::IsFalse  (p.IsCaughtUp());
        }


        TEST_METHOD (CoalesceJumpCutsPastLargeBacklog)
        {
            // Backlog (5000) far exceeds the 500-row coalesce threshold: snap.
            PrinterPacing   p (Cfg (1280.0, 16, 500.0));

            p.Reset (0.0);
            p.SetTargetRows (5000);

            Assert::AreEqual (5000, p.Advance (0.1));
            Assert::IsTrue   (p.IsCaughtUp());
        }


        TEST_METHOD (SmallBacklogSweepsNotJumpCut)
        {
            // Backlog (300) is under the 500 threshold: sweep, do not snap.
            PrinterPacing   p (Cfg (1280.0, 16, 500.0));

            p.Reset (0.0);
            p.SetTargetRows (300);

            Assert::AreEqual (16, p.Advance (1.0));    // one band, still animating
            Assert::IsFalse  (p.IsCaughtUp());
        }


        TEST_METHOD (BackwardClockMakesNoProgress)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (1000);

            Assert::AreEqual (80, p.Advance (5.0));    // 5 passes -> 80 rows
            Assert::AreEqual (80, p.Advance (3.0));    // time went back -> no change
            Assert::AreEqual (96, p.Advance (4.0));    // resumes from the t=3 baseline
        }


        TEST_METHOD (ResetWithRevealedRows)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (10.0, 400);
            p.SetTargetRows (1000);

            Assert::AreEqual (400, p.RevealedRows());
            Assert::AreEqual (416, p.Advance (11.0));   // +1 band in 1 s
        }


        TEST_METHOD (ShrinkingTargetClampsReveal)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (1000);
            Assert::AreEqual (80, p.Advance (5.0));

            p.SetTargetRows (50);                       // target shrank below reveal
            Assert::AreEqual (50, p.RevealedRows());
            Assert::IsTrue   (p.IsCaughtUp());
        }


        TEST_METHOD (FirstAdvanceEstablishesClockBaseline)
        {
            // Without Reset, the first Advance sets the baseline (dt == 0), so no
            // giant jump from a large absolute timestamp.
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.SetTargetRows (1000);

            Assert::AreEqual (0,  p.Advance (12345.0));   // baseline, no progress
            Assert::AreEqual (16, p.Advance (12346.0));   // +1 s -> 1 band
        }


        TEST_METHOD (StallResumesWithoutLeapingButSlowFramesAreNotThrottled)
        {
            // Two things at once. (1) A long idle-loop stall (huge dt) must NOT
            // leap a whole pass on resume -- it nudges the head just off the
            // margin so the sweep re-arms and animates the rest. (2) A merely-slow
            // frame (dt under the resume threshold) is NOT clamped -- it advances
            // the full carriage speed for its dt, so a low frame rate never drags
            // the sweep slow.
            PrinterPacing::Config   c;
            c.dotsPerSecond          = 1280.0;
            c.rowsPerSweep           = 16;
            c.coalesceRows           = 1.0e9;
            c.resumeThresholdSeconds = 0.20;
            c.resumeNudgeSeconds     = 0.02;

            PrinterPacing   p (c);
            p.Reset (0.0);
            p.SetTargetRows (1000);

            Assert::AreEqual (0, p.Advance (10.0));      // 10s stall -> resume nudge, no pass completes
            int  afterStall = p.RevealedColDots();
            Assert::IsTrue   (afterStall > 0 && afterStall < 200);   // just off the margin, nowhere near a full pass
            Assert::IsFalse  (p.IsCaughtUp());

            // A slow 10 fps frame (dt 0.1 < 0.2 threshold) is unthrottled: full
            // carriage speed is 1280 * 0.1 = 128 dots, not a clamped sliver.
            p.Advance (10.1);
            Assert::IsTrue   (p.RevealedColDots() - afterStall > 100);
        }


        TEST_METHOD (ShortLineSweepsOnlyItsExtent)
        {
            // Logic seeking: the pass spans the live band's ink extent, not the
            // whole platen. At 1280 dots/s a 320-dot line completes a band
            // every quarter second -- 4x the full-width rate -- and the column
            // never exceeds the sweep width.
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetPosition (1000, 320);

            Assert::AreEqual (320, p.SweepWidthDots());
            Assert::AreEqual (16,  p.Advance (0.25));    // one short pass == one band
            Assert::AreEqual (80,  p.Advance (1.25));    // four more passes in the next second
            Assert::IsTrue   (p.RevealedColDots() <= 320);
        }


        TEST_METHOD (NarrowerBandClampsAMidPassColumn)
        {
            // Mid-pass, the guest's next band turns out narrower: the column
            // clamps to the new sweep width (finishing the pass at its edge)
            // instead of overshooting past it.
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetPosition (1000, 1280);
            p.Advance (0.5);                             // mid-pass at col 640

            p.SetTargetPosition (1000, 320);             // narrower live band
            Assert::AreEqual (320, p.RevealedColDots()); // clamped to the pass edge
        }


        TEST_METHOD (BlankBandFeedsWithTheHeadParked)
        {
            // A blank live band (line / form feed) is a paper SLEW, not a
            // print: rows advance at blankRowsPerSecond while the head stays
            // parked and the direction never flips. (A form feed used to sweep
            // the carriage across ~99 empty bands of the page.)
            PrinterPacing::Config   c = Cfg (1280.0, 16, 1.0e9);

            c.blankRowsPerSecond = 480.0;

            PrinterPacing   p (c);

            p.Reset (0.0);
            p.SetTargetRows (1000);

            Assert::AreEqual (480, p.Advance (1.0, false));   // slewed a page-chunk in 1 s
            Assert::AreEqual (0,   p.RevealedColDots());      // head parked at its margin
            Assert::IsTrue   (p.SweepLeftToRight());          // no spurious direction flips
        }


        TEST_METHOD (InkAfterBlankFeedResumesTheSweep)
        {
            PrinterPacing::Config   c = Cfg (1280.0, 16, 1.0e9);

            c.blankRowsPerSecond = 480.0;

            PrinterPacing   p (c);

            p.Reset (0.0);
            p.SetTargetRows (1000);
            p.Advance (1.0, false);                    // blank stretch slews through

            Assert::AreEqual (480, p.RevealedRows());
            Assert::AreEqual (480, p.Advance (1.25, true));   // inked band: back to sweeping...
            Assert::AreEqual (320, p.RevealedColDots());      // ...from the parked margin
        }


        TEST_METHOD (SweepDirectionReversesEachBand)
        {
            // The ImageWriter prints bidirectionally: the carriage direction flips
            // every time a fresh band's pass begins (on sweep completion).
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0, 10);
            p.SetTargetRows (1000);
            Assert::IsTrue  (p.SweepLeftToRight());    // starts left-to-right

            p.Advance (1.0);                            // first pass completes -> reverse
            Assert::IsFalse (p.SweepLeftToRight());

            p.Advance (2.0);                            // next pass -> reverse again
            Assert::IsTrue  (p.SweepLeftToRight());
        }


        TEST_METHOD (FastForwardParksColumnAtHome)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0, 10);
            p.SetTargetRows (500);
            p.RequestFastForward();

            p.Advance (0.01);
            Assert::AreEqual (500, p.RevealedRows());
            Assert::AreEqual (0, p.RevealedColDots());   // parked at home after the jump-cut
        }
    };
}
