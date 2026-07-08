#include "Pch.h"

#include "Devices/Printer/PrinterPacing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Clock-driven pacing for the printer panel's paper animation (R-012). The
// reveal is deterministic under an injected clock: a steady rate, a fast-forward
// skip, and a coalescing jump-cut past large backlogs. Rates/thresholds are set
// explicitly per test so behavior -- not the tuned default constants -- is what
// is asserted.
namespace PrinterPacingTests
{
    static PrinterPacing::Config Cfg (double rate, double coalesce)
    {
        PrinterPacing::Config   c;
        c.rowsPerSecond = rate;
        c.coalesceRows  = coalesce;
        return c;
    }


    TEST_CLASS (PrinterPacingTests)
    {
    public:

        TEST_METHOD (RevealsAtSteadyRate)
        {
            // 100 rows/s, coalesce disabled (huge threshold) so we see the crawl.
            PrinterPacing   p (Cfg (100.0, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (1000);

            Assert::AreEqual (100, p.Advance (1.0));
            Assert::AreEqual (250, p.Advance (2.5));
            Assert::AreEqual (300, p.Advance (3.0));
            Assert::IsFalse  (p.IsCaughtUp ());
        }


        TEST_METHOD (ClampsToTargetAndCatchesUp)
        {
            PrinterPacing   p (Cfg (100.0, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (150);

            Assert::AreEqual (100, p.Advance (1.0));
            Assert::AreEqual (150, p.Advance (2.0));   // would be 200, clamped
            Assert::AreEqual (150, p.Advance (5.0));   // stays put
            Assert::IsTrue   (p.IsCaughtUp ());
        }


        TEST_METHOD (FastForwardRevealsEverythingNextAdvance)
        {
            PrinterPacing   p (Cfg (100.0, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (5000);
            p.RequestFastForward ();

            Assert::AreEqual (5000, p.Advance (0.01));   // tiny dt, still all revealed
            Assert::IsTrue   (p.IsCaughtUp ());
        }


        TEST_METHOD (FastForwardIsOneShot)
        {
            PrinterPacing   p (Cfg (100.0, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (200);
            p.RequestFastForward ();

            Assert::AreEqual (200, p.Advance (0.01));

            // Grow the job; the earlier fast-forward must not still be latched.
            p.SetTargetRows (400);
            Assert::AreEqual (200, p.Advance (0.01));   // ~1 row of new reveal, clamps low
            Assert::IsFalse  (p.IsCaughtUp ());
        }


        TEST_METHOD (CoalesceJumpCutsPastLargeBacklog)
        {
            // Backlog (5000) far exceeds the 500-row coalesce threshold: snap.
            PrinterPacing   p (Cfg (100.0, 500.0));

            p.Reset (0.0);
            p.SetTargetRows (5000);

            Assert::AreEqual (5000, p.Advance (0.1));
            Assert::IsTrue   (p.IsCaughtUp ());
        }


        TEST_METHOD (SmallBacklogAnimatesNotJumpCut)
        {
            // Backlog (300) is under the 500 threshold: crawl, do not snap.
            PrinterPacing   p (Cfg (100.0, 500.0));

            p.Reset (0.0);
            p.SetTargetRows (300);

            Assert::AreEqual (100, p.Advance (1.0));
            Assert::IsFalse  (p.IsCaughtUp ());
        }


        TEST_METHOD (BackwardClockMakesNoProgress)
        {
            PrinterPacing   p (Cfg (100.0, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (1000);

            Assert::AreEqual (500, p.Advance (5.0));
            Assert::AreEqual (500, p.Advance (3.0));   // time went back -> no change
            Assert::AreEqual (600, p.Advance (4.0));   // resumes from last time
        }


        TEST_METHOD (ResetWithRevealedRows)
        {
            PrinterPacing   p (Cfg (100.0, 1.0e9));

            p.Reset (10.0, 400);
            p.SetTargetRows (1000);

            Assert::AreEqual (400, p.RevealedRows ());
            Assert::AreEqual (500, p.Advance (11.0));   // +100 rows in 1s
        }


        TEST_METHOD (ShrinkingTargetClampsReveal)
        {
            PrinterPacing   p (Cfg (100.0, 1.0e9));

            p.Reset (0.0);
            p.SetTargetRows (1000);
            Assert::AreEqual (500, p.Advance (5.0));

            p.SetTargetRows (300);                      // target shrank below reveal
            Assert::AreEqual (300, p.RevealedRows ());
            Assert::IsTrue   (p.IsCaughtUp ());
        }


        TEST_METHOD (FirstAdvanceEstablishesClockBaseline)
        {
            // Without Reset, the first Advance sets the baseline (dt == 0), so no
            // giant jump from a large absolute timestamp.
            PrinterPacing   p (Cfg (100.0, 1.0e9));

            p.SetTargetRows (1000);

            Assert::AreEqual (0,   p.Advance (12345.0));   // baseline, no progress
            Assert::AreEqual (100, p.Advance (12346.0));   // +1s
        }
    };
}
