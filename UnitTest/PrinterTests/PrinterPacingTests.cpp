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
        c.dotsPerSecond = dotsPerSec;
        c.rowsPerSweep  = rowsPerSweep;
        c.coalesceRows  = coalesce;
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


        TEST_METHOD (CaughtUpShowsTheLiveBandComplete)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (16);

            p.Advance (1.0);                           // one pass reveals the band
            Assert::AreEqual (16, p.RevealedRows());
            Assert::IsTrue   (p.IsCaughtUp());
            Assert::AreEqual (PrinterGrid::kDotsPerRow, p.RevealedColDots());
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
            Assert::AreEqual (PrinterGrid::kDotsPerRow, p.RevealedColDots());
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


        TEST_METHOD (FastForwardSnapsColumnToo)
        {
            PrinterPacing   p (Cfg (1280.0, 16, 1.0e9));

            p.Reset (0.0, 10);
            p.SetTargetRows (500);
            p.RequestFastForward();

            p.Advance (0.01);
            Assert::AreEqual (500, p.RevealedRows());
            Assert::AreEqual (PrinterGrid::kDotsPerRow, p.RevealedColDots());
        }
    };
}
